"""Explicit, local-first workstream synchronization. No unattended execution."""

import argparse
from contextlib import contextmanager
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time
from urllib.parse import quote


ROOT = Path(__file__).resolve().parents[1]
FIELDS = (
    "Status", "Milestone", "Dependencies", "Purpose", "Tasks",
    "Acceptance criteria", "Completion evidence", "Completion log",
)
ISSUE_FIELDS = "id number title body state stateReason url"
ITEM_FIELDS = """
id isArchived content { ... on Issue { id } }
fieldValueByName(name:"Status") {
  ... on ProjectV2ItemFieldSingleSelectValue { optionId }
}
"""


class SyncError(Exception):
    pass


def digest(text):
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def load_json(path):
    with path.open(encoding="utf-8") as source:
        return json.load(source)


def save_json(path, value):
    write_atomic(path, json.dumps(value, indent=2, ensure_ascii=True) + "\n")


def write_atomic(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    name = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="w", encoding="utf-8", newline="\n",
            dir=path.parent, prefix=path.name + ".", suffix=".tmp", delete=False,
        ) as target:
            name = Path(target.name)
            target.write(text)
            target.flush()
            os.fsync(target.fileno())
        os.replace(name, path)
    finally:
        if name is not None and name.exists():
            name.unlink()


@contextmanager
def writer_lock(root):
    path = root / ".todo-sync.lock"
    try:
        descriptor = os.open(path, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
    except FileExistsError as error:
        raise SyncError("Writer lock exists; inspect its PID before manual recovery.") from error
    try:
        with os.fdopen(descriptor, "w") as target:
            target.write(str(os.getpid()))
        yield
    finally:
        path.unlink()


class GitHub:
    def request(self, query, **variables):
        mutation = query.lstrip().startswith("mutation")
        for attempt in range(3):
            try:
                result = subprocess.run(
                    ["gh", "api", "graphql", "--input", "-"],
                    input=json.dumps({"query": query, "variables": variables}),
                    capture_output=True, text=True, encoding="utf-8", timeout=120,
                )
            except subprocess.TimeoutExpired as error:
                if mutation or attempt == 2:
                    raise SyncError("GitHub timeout; mutation outcome may be uncertain. Rerun to reconcile.") from error
                time.sleep(attempt + 1)
                continue
            if result.returncode:
                message = result.stderr.strip()
                for key in ("GH_TOKEN", "TODO_SYNC_GITHUB_TOKEN"):
                    if os.environ.get(key):
                        message = message.replace(os.environ[key], "[redacted]")
                transient = any(word in message.lower() for word in (
                    "timeout", "timed out", "502", "503", "rate limit", "connection",
                ))
                if not mutation and transient and attempt < 2:
                    time.sleep(attempt + 1)
                    continue
                raise SyncError("GitHub request failed: " + message[:800])
            value = json.loads(result.stdout)
            if value.get("errors") or value.get("data") is None:
                raise SyncError("GitHub returned GraphQL errors or missing data; no empty baseline assumed.")
            return value["data"]
        raise SyncError("GitHub read retries exhausted.")

    def connection(self, query, route, **variables):
        nodes = []
        cursor = None
        seen = set()
        while True:
            value = self.request(query, cursor=cursor, **variables)
            for key in route:
                if value is None:
                    raise SyncError("Missing or inaccessible GitHub connection.")
                value = value[key]
            if value is None:
                raise SyncError("Missing GitHub connection.")
            nodes.extend(value["nodes"])
            page = value["pageInfo"]
            if not page["hasNextPage"]:
                return nodes
            cursor = page["endCursor"]
            if not cursor or cursor in seen:
                raise SyncError("Invalid GitHub pagination cursor.")
            seen.add(cursor)

    def inventory(self, config):
        owner, name = config["repository"].split("/")
        metadata = self.request("""
query($owner:String!,$name:String!,$project:ID!) {
  viewer { login }
  repository(owner:$owner,name:$name) { id nameWithOwner isPrivate viewerPermission }
  node(id:$project) { ... on ProjectV2 {
    id title url public viewerCanUpdate
    repositories(first:100) { nodes { id } pageInfo { hasNextPage } }
    fields(first:100) { nodes {
      ... on ProjectV2SingleSelectField { id name options { id name } }
    } pageInfo { hasNextPage } }
  } }
}""", owner=owner, name=name, project=config["project"]["id"])
        repo, project = metadata["repository"], metadata["node"]
        if not repo or repo["id"] != config["repository_id"]:
            raise SyncError("Repository identity changed or is inaccessible.")
        if repo["nameWithOwner"] != config["repository"] or repo["isPrivate"]:
            raise SyncError("Repository rename/visibility needs reviewed configuration.")
        if not project or project["public"] != config["project"]["public"]:
            raise SyncError("Project identity/visibility needs review.")
        if not project["viewerCanUpdate"] or repo["viewerPermission"] not in ("WRITE", "MAINTAIN", "ADMIN"):
            raise SyncError("Issue/Project write permission unavailable.")
        if project["fields"]["pageInfo"]["hasNextPage"] or project["repositories"]["pageInfo"]["hasNextPage"]:
            raise SyncError("Project field/repository inventory exceeds bounded metadata read.")
        if config["repository_id"] not in [node["id"] for node in project["repositories"]["nodes"]]:
            raise SyncError("Configured Project is no longer linked to the repository.")
        status = [field for field in project["fields"]["nodes"]
                  if field.get("id") == config["project"]["status_field_id"]]
        if len(status) != 1 or status[0]["name"] != "Status":
            raise SyncError("Configured Status field missing or renamed.")
        options = {option["id"]: option["name"] for option in status[0]["options"]}
        for stage, identifier in config["project"]["options"].items():
            if options.get(identifier) != config["project"]["option_names"][stage]:
                raise SyncError("Status option changed: " + stage)
        issues = self.connection("""
query($owner:String!,$name:String!,$cursor:String) {
 repository(owner:$owner,name:$name) {
   issues(first:100,after:$cursor,states:[OPEN,CLOSED]) {
     nodes { """ + ISSUE_FIELDS + """ } pageInfo { hasNextPage endCursor }
   }
 }
}""", ("repository", "issues"), owner=owner, name=name)
        items = self.connection("""
query($project:ID!,$cursor:String) { node(id:$project) { ... on ProjectV2 {
 items(first:100,after:$cursor) { nodes { """ + ITEM_FIELDS + """ }
 pageInfo { hasNextPage endCursor } }
} } }""", ("node", "items"), project=project["id"])
        return metadata["viewer"]["login"], issues, items

    def issue(self, identifier):
        value = self.request(
            "query($id:ID!) { node(id:$id) { ... on Issue { " + ISSUE_FIELDS + " } } }",
            id=identifier,
        )["node"]
        if not value:
            raise SyncError("Mapped issue disappeared or became inaccessible.")
        return value

    def item(self, identifier):
        value = self.request(
            "query($id:ID!) { node(id:$id) { ... on ProjectV2Item { " + ITEM_FIELDS + " } } }",
            id=identifier,
        )["node"]
        if not value:
            raise SyncError("Mapped Project item disappeared or became inaccessible.")
        return value

    def create_issue(self, repository, title, body):
        return self.request("""
mutation($repo:ID!,$title:String!,$body:String!) {
 createIssue(input:{repositoryId:$repo,title:$title,body:$body}) { issue {
""" + ISSUE_FIELDS + """ } } }""", repo=repository, title=title, body=body)["createIssue"]["issue"]

    def update_issue(self, issue, title, body):
        self.request("""
mutation($id:ID!,$title:String!,$body:String!) {
 updateIssue(input:{id:$id,title:$title,body:$body}) { issue { id } }
}""", id=issue, title=title, body=body)

    def lifecycle(self, issue, state, reason):
        self.request("""mutation($id:ID!,$state:IssueState!,$reason:IssueClosedStateReason) {
 updateIssue(input:{id:$id,stateInput:{value:$state,stateReason:$reason}}) { issue { id } }
}""", id=issue, state=state, reason=reason)

    def add_item(self, project, issue):
        return self.request("""mutation($project:ID!,$issue:ID!) {
 addProjectV2ItemById(input:{projectId:$project,contentId:$issue}) { item { id } }
}""", project=project, issue=issue)["addProjectV2ItemById"]["item"]["id"]

    def set_status(self, project, item, field, option):
        self.request("""mutation($project:ID!,$item:ID!,$field:ID!,$option:String!) {
 updateProjectV2ItemFieldValue(input:{
 projectId:$project,itemId:$item,fieldId:$field,value:{singleSelectOptionId:$option}
 }) { projectV2Item { id } }
}""", project=project, item=item, field=field, option=option)


def markers(identifier):
    return (f"<!-- windowpane:{identifier}:begin -->",
            f"<!-- windowpane:{identifier}:end -->")


def managed_section(body, identifier):
    begin, end = markers(identifier)
    if body.count(begin) != 1 or body.count(end) != 1:
        raise SyncError("Managed issue markers missing or duplicated.")
    start = body.index(begin)
    if body.index(end) < start:
        raise SyncError("Managed issue markers out of order.")
    stop = body.index(end) + len(end)
    return body[start:stop]


def parse_workstream(path, identifier, config, root):
    text = path.read_text(encoding="utf-8")
    for key, expected in (("Workstream-ID", identifier),):
        matches = re.findall(rf"^{key}: (.+)$", text, re.M)
        if matches and matches != [expected]:
            raise SyncError("Local stable identity mismatch: " + str(path))
    clean = re.sub(r"^(Workstream-ID|GitHub-Issue):[^\n]*\n", "", text, flags=re.M)
    headings = list(re.finditer(
        r"^(" + "|".join(re.escape(field) for field in FIELDS) + r"):[ \t]*(.*)$",
        clean, re.M,
    ))
    if [match[1] for match in headings] != list(FIELDS):
        raise SyncError("All eight fields must occur once, in contract order: " + path.name)
    values = {}
    for index, match in enumerate(headings):
        end = headings[index + 1].start() if index + 1 < len(headings) else len(clean)
        values[match[1]] = (match[2] + "\n" + clean[match.end():end]).strip()
    status_text = values["Status"]
    status = config["reviewed_statuses"].get(status_text, status_text)
    if status == "BLOCKED":
        stage = re.search(r"^Blocked-stage: (.+)$", text, re.M)
        blocker = re.search(r"^Blocker: (\S.*)$", text, re.M)
        if not stage or not blocker or stage[1] not in ("Backlog", "Ready", "In progress", "In review"):
            raise SyncError("BLOCKED needs explicit Blocked-stage and Blocker metadata.")
        state, reason, stage = "OPEN", None, stage[1]
    elif status in config["status_map"]:
        state, reason, stage = config["status_map"][status]
    else:
        raise SyncError("Unreviewed exact Status value: " + status_text)
    title = clean[:headings[0].start()].strip().splitlines()[0]
    begin, end = markers(identifier)
    relative = path.relative_to(root).as_posix()
    source = f"https://github.com/{config['repository']}/blob/{config['branch']}/{quote(relative)}"
    sections = [
        begin, f"Source workstream: [{relative}]({source})",
        "Local workstream owns this section. Historical evidence/dates are retained; "
        "GitHub metadata records actual synchronization dates. Local paths are projected "
        "relative to the repository. Completion does not imply publication or user approval.",
    ]
    for field in FIELDS:
        value = values[field]
        for prefix in (str(root) + "\\", str(root).replace("\\", "/") + "/"):
            value = value.replace(prefix, "")
        sections.append(f"### {field}\n{value}")
    if status == "BLOCKED":
        sections.append(f"Blocked stage: {stage}\nBlocker: {blocker[1]}")
    sections.append(end)
    body = "\n\n".join(sections)
    if re.search(r"\b(?:gh[pousr]_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{20,})", body):
        raise SyncError("Token-shaped content blocked; review source without publishing it.")
    if re.search(r"[A-Za-z]:[\\/]Users[\\/]", body, re.I):
        raise SyncError("Personal absolute path blocked; explicitly review a public-safe projection.")
    if len(body) > 60000:
        raise SyncError("Managed body exceeds safe GitHub size.")
    return {
        "title": f"[{identifier[3:]}] {title}", "body": body,
        "managed": digest(body), "lifecycle": [state, reason],
        "status": config["project"]["options"][stage], "stage": stage,
    }


def snapshot(issue, item, identifier):
    return {
        "title": issue["title"],
        "managed": digest(managed_section(issue["body"], identifier)),
        "lifecycle": [issue["state"], issue["stateReason"] if issue["state"] == "CLOSED" else None],
        "status": item["fieldValueByName"]["optionId"] if item and item["fieldValueByName"] else None,
    }


def desired_snapshot(work):
    return {key: work[key] for key in ("title", "managed", "lifecycle", "status")}


def check_conflicts(current, record):
    baseline = record.get("baseline")
    if baseline is None:
        raise SyncError("No verified baseline; explicit reconciliation required.")
    pending = record.get("pending", {})
    for key, value in current.items():
        if value != baseline[key] and value != pending.get(key, baseline[key]):
            raise SyncError("Remote human edit conflicts with managed " + key + ".")


def write_backlink(path, identifier, url):
    text = path.read_text(encoding="utf-8")
    for key, value in (("Workstream-ID", identifier), ("GitHub-Issue", url)):
        matches = re.findall(rf"^{key}: (.+)$", text, re.M)
        if matches and matches != [value]:
            raise SyncError("Conflicting local " + key + ".")
    clean = re.sub(r"^(Workstream-ID|GitHub-Issue):[^\n]*\n", "", text, flags=re.M)
    first, rest = clean.split("\n", 1)
    updated = first + f"\nWorkstream-ID: {identifier}\nGitHub-Issue: {url}\n" + rest
    if updated != text:
        # Preserve every workstream field; only two metadata lines are managed.
        write_atomic(path, updated)


def find_issue(issues, identifier, record):
    begin, _ = markers(identifier)
    matches = [issue for issue in issues if begin in issue["body"]]
    if len(matches) > 1:
        raise SyncError("Duplicate stable issue identity; reconcile without deleting.")
    if record.get("issue_id"):
        if len(matches) != 1 or matches[0]["id"] != record["issue_id"]:
            raise SyncError("Mapped issue/marker disappeared or changed; no replacement created.")
    elif matches and not record.get("creating"):
        raise SyncError("Unmapped stable marker exists; explicit adoption required.")
    return matches[0] if matches else None


def find_item(items, issue, record):
    matches = [item for item in items if item.get("content") and item["content"].get("id") == issue["id"]]
    if len(matches) > 1:
        raise SyncError("Duplicate Project membership.")
    item = matches[0] if matches else None
    if record.get("item_id") and (not item or item["id"] != record["item_id"]):
        raise SyncError("Mapped Project item removed/changed; no replacement created.")
    if item and item["isArchived"]:
        raise SyncError("Project item was archived; human reconciliation required.")
    return item


def plan(identifier, work, issue, item, record, path):
    local_text = path.read_text(encoding="utf-8")
    links = re.findall(r"^GitHub-Issue: (.+)$", local_text, re.M)
    if not issue:
        if links:
            raise SyncError("Local backlink has no discovered mapped issue; reconcile before creating.")
        if record.get("creating"):
            raise SyncError("Uncertain issue create not visible yet; inspect before retrying.")
        actions = ["create issue", "add Project item", "set " + work["stage"],
                   "close/" + work["lifecycle"][1] if work["lifecycle"][0] == "CLOSED" else "keep open"]
    else:
        current = snapshot(issue, item, identifier)
        if record.get("creating") and not record.get("baseline"):
            expected = record["creating"]
            if current["title"] != expected["title"] or current["managed"] != expected["managed"]:
                raise SyncError("Recovered create differs from saved intent.")
            if current["lifecycle"] != ["OPEN", None]:
                raise SyncError("Recovered new issue has unexpected lifecycle.")
        else:
            check_conflicts(current, record)
        actions = []
        target = desired_snapshot(work)
        for key in ("title", "managed", "lifecycle", "status"):
            if current[key] != target[key]:
                actions.append("update " + key)
        if not item:
            if record.get("adding_item"):
                raise SyncError("Uncertain Project add not visible yet; inspect before retrying.")
            actions.append("add Project item")
    if issue:
        expected_link = "GitHub-Issue: " + issue["url"]
        if links and links != [issue["url"]]:
            raise SyncError("Conflicting local backlink.")
        if expected_link not in local_text or f"Workstream-ID: {identifier}" not in local_text:
            actions.append("write backlink")
    else:
        actions.append("write backlink")
    return actions


def synchronize(api, config, state, persist, identifier, path, work, issue, item, issues, items):
    record = state["workstreams"].setdefault(identifier, {})
    if issue:
        issue = api.issue(issue["id"])
        item = api.item(item["id"]) if item else None
        if item and (item["isArchived"] or item["content"].get("id") != issue["id"]):
            raise SyncError("Project membership changed before apply.")
        plan(identifier, work, issue, item, record, path)
    else:
        record["creating"] = {"title": work["title"], "managed": work["managed"]}
        persist()
        issue = api.create_issue(config["repository_id"], work["title"], work["body"])
        issues.append(issue)
    if not record.get("baseline"):
        record["baseline"] = snapshot(issue, item, identifier)
    record.update(issue_id=issue["id"], number=issue["number"], url=issue["url"])
    record.pop("creating", None)
    record["pending"] = desired_snapshot(work)
    persist()
    current = snapshot(issue, item, identifier)
    if current["title"] != work["title"] or current["managed"] != work["managed"]:
        old = managed_section(issue["body"], identifier)
        body = issue["body"].replace(old, work["body"], 1)
        api.update_issue(issue["id"], work["title"], body)
    if current["lifecycle"] != work["lifecycle"]:
        api.lifecycle(issue["id"], *work["lifecycle"])
    if not item:
        record["adding_item"] = True
        persist()
        item_id = api.add_item(config["project"]["id"], issue["id"])
        record["item_id"] = item_id
        record.pop("adding_item", None)
        persist()
        item = api.item(item_id)
        items.append(item)
    else:
        record["item_id"] = item["id"]
        record.pop("adding_item", None)
        persist()
    if snapshot(issue, item, identifier)["status"] != work["status"]:
        api.set_status(config["project"]["id"], item["id"],
                       config["project"]["status_field_id"], work["status"])
    verified_issue, verified_item = api.issue(issue["id"]), api.item(item["id"])
    if verified_item["isArchived"] or verified_item["content"].get("id") != issue["id"]:
        raise SyncError("Read-back Project membership mismatch.")
    actual = snapshot(verified_issue, verified_item, identifier)
    if actual != desired_snapshot(work):
        raise SyncError("Read-back differs; preserve pending operation and rerun after visibility settles.")
    write_backlink(path, identifier, issue["url"])
    text = path.read_text(encoding="utf-8")
    if f"GitHub-Issue: {issue['url']}" not in text or f"Workstream-ID: {identifier}" not in text:
        raise SyncError("Persistent local backlink verification failed.")
    record["baseline"] = actual
    record.pop("pending", None)
    persist()
    return verified_issue


def run(config_path, apply=False, selected=None, api=None, root=ROOT):
    config = load_json(config_path)
    if config["schema"] != 1:
        raise SyncError("Unsupported configuration schema.")
    names = list(config["workstreams"].values())
    if len(names) != len(set(names)) or set(names) & set(config["exclude"]):
        raise SyncError("Duplicate or excluded configured workstream.")
    for identifier, name in config["workstreams"].items():
        if not re.fullmatch(r"wp-\d{2}", identifier) or Path(name).name != name or not name.endswith(".todo"):
            raise SyncError("Invalid stable identifier or workstream filename.")
    selected = selected or list(config["workstreams"])
    if set(selected) - set(config["workstreams"]):
        raise SyncError("Unknown selected workstream.")
    state_path = root / config["state_file"]
    state = load_json(state_path) if state_path.exists() else {
        "schema": 1, "repository_id": config["repository_id"],
        "project_id": config["project"]["id"], "workstreams": {},
    }
    if (state["schema"] != 1 or state["repository_id"] != config["repository_id"]
            or state["project_id"] != config["project"]["id"]):
        raise SyncError("Mapping identity mismatch.")
    api = api or GitHub()
    viewer, issues, items = api.inventory(config)
    print(f"{'APPLY' if apply else 'PREVIEW'} {config['repository']} as {viewer}; {config['project']['url']}")
    print(f"Inventory: {len(issues)} issues, {len(items)} Project items; {len(selected)} selected.")
    for name, why in config["exclude"].items():
        print(f"EXCLUDED {name}: {why}")
    known = set(names) | set(config["exclude"])
    for path in sorted((root / "TODO").glob("*.todo")):
        if path.name not in known:
            print(f"UNADOPTED {path.name}: public-content and status review required; no writes.")
    for identifier in set(state["workstreams"]) - set(config["workstreams"]):
        print(f"ORPHANED {identifier}: mapping retained; no remote deletion.")
    errors = 0
    counts = {"created": 0, "reused": 0, "open": 0, "closed": 0, "verified": 0, "no_op": 0}
    for identifier in selected:
        try:
            path = root / "TODO" / config["workstreams"][identifier]
            work = parse_workstream(path, identifier, config, root)
            record = state["workstreams"].get(identifier, {})
            issue = find_issue(issues, identifier, record)
            item = find_item(items, issue, record) if issue else None
            actions = plan(identifier, work, issue, item, record, path)
            print(f"{identifier} {path.name} {work['title']}: "
                  f"{', '.join(actions) if actions else 'no-op'} -> {work['lifecycle']} / {work['stage']}")
            if apply:
                existed = bool(issue)
                issue = synchronize(
                    api, config, state, lambda: save_json(state_path, state),
                    identifier, path, work, issue, item, issues, items,
                )
                counts["reused" if existed else "created"] += 1
                counts["open" if issue["state"] == "OPEN" else "closed"] += 1
                counts["verified"] += 1
                counts["no_op"] += not actions
        except (SyncError, OSError, ValueError, KeyError) as error:
            errors += 1
            print(f"BLOCKED {identifier}: {error}", file=sys.stderr)
    print(json.dumps({"results": counts, "blocked": errors}, sort_keys=True))
    return 1 if errors else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, default=ROOT / "scripts" / "todo-sync.json")
    parser.add_argument("--apply", action="store_true", help="Explicitly allow issue/Project/backlink writes")
    parser.add_argument("--only", action="append", help="Stable workstream ID; repeat to select multiple")
    args = parser.parse_args()
    try:
        if args.apply:
            with writer_lock(ROOT):
                return run(args.config, True, args.only)
        return run(args.config, False, args.only)
    except (SyncError, OSError, ValueError, KeyError) as error:
        print(f"BLOCKED: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
