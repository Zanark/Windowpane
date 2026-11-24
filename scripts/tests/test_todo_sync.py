import copy
from contextlib import redirect_stderr, redirect_stdout
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch


SCRIPT = Path(__file__).resolve().parents[1] / "todo_sync.py"
SPEC = importlib.util.spec_from_file_location("todo_sync", SCRIPT)
sync = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(sync)


class FakeGitHub:
    def __init__(self):
        self.issues = []
        self.items = []
        self.writes = []
        self.fail_add = False
        self.timeout_create = False
        self.fail_inventory = False

    def inventory(self, config):
        if self.fail_inventory:
            raise sync.SyncError("permission/read failure")
        return "fixture", copy.deepcopy(self.issues), copy.deepcopy(self.items)

    def issue(self, identifier):
        return copy.deepcopy(next(item for item in self.issues if item["id"] == identifier))

    def item(self, identifier):
        return copy.deepcopy(next(item for item in self.items if item["id"] == identifier))

    def create_issue(self, repository, title, body):
        number = len(self.issues) + 1
        issue = {
            "id": f"I{number}", "number": number, "title": title, "body": body,
            "state": "OPEN", "stateReason": None,
            "url": f"https://github.com/example/fixture/issues/{number}",
        }
        self.issues.append(issue)
        self.writes.append("create")
        if self.timeout_create:
            self.timeout_create = False
            raise sync.SyncError("uncertain create timeout")
        return copy.deepcopy(issue)

    def update_issue(self, identifier, title, body):
        issue = next(item for item in self.issues if item["id"] == identifier)
        issue.update(title=title, body=body)
        self.writes.append("body")

    def lifecycle(self, identifier, state, reason):
        issue = next(item for item in self.issues if item["id"] == identifier)
        issue.update(state=state, stateReason=reason)
        self.writes.append("lifecycle")

    def add_item(self, project, issue):
        if self.fail_add:
            self.fail_add = False
            raise sync.SyncError("Project permission failure")
        item = {
            "id": f"P{len(self.items) + 1}", "isArchived": False,
            "content": {"id": issue}, "fieldValueByName": None,
        }
        self.items.append(item)
        self.writes.append("add")
        return item["id"]

    def set_status(self, project, identifier, field, option):
        item = next(item for item in self.items if item["id"] == identifier)
        item["fieldValueByName"] = {"optionId": option}
        self.writes.append("status")


class SynchronizationTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        (self.root / "TODO").mkdir()
        self.config = sync.load_json(SCRIPT.with_name("todo-sync.json"))
        self.config["state_file"] = "state.json"
        self.config["workstreams"] = {"wp-01": "01_FOUNDATION.todo"}
        self.config_path = self.root / "config.json"
        sync.save_json(self.config_path, self.config)
        self.path = self.root / "TODO" / "01_FOUNDATION.todo"
        self.initial = (
            "Fixture workstream\n\nStatus: DONE\nMilestone: Bounded outcome\n"
            "Dependencies: None\nPurpose: Public fixture\n\nTasks:\n- [x] Actual task\n\n"
            "Acceptance criteria:\n- Bounded result\n\nCompletion evidence:\n"
            "- Synthetic evidence, not human acceptance\n\nCompletion log:\n- 2020-01-01: Actual history\n"
        )
        self.path.write_text(self.initial, encoding="utf-8")
        self.api = FakeGitHub()

    def execute(self, apply=True, selected=None):
        self.output = io.StringIO()
        with redirect_stdout(self.output), redirect_stderr(self.output):
            return sync.run(self.config_path, apply, selected, self.api, self.root)

    def state(self):
        return sync.load_json(self.root / "state.json")

    def change(self, before, after):
        text = self.path.read_text(encoding="utf-8").replace(before, after)
        self.path.write_text(text, encoding="utf-8")

    def test_preview_has_no_remote_or_local_writes(self):
        self.assertEqual(self.execute(False), 0)
        self.assertEqual(self.api.writes, [])
        self.assertEqual(self.path.read_text(encoding="utf-8"), self.initial)
        self.assertFalse((self.root / "state.json").exists())

    def test_done_backlink_and_idempotence(self):
        self.assertEqual(self.execute(), 0)
        self.assertEqual(self.api.issues[0]["stateReason"], "COMPLETED")
        self.assertEqual(self.api.items[0]["content"]["id"], self.api.issues[0]["id"])
        self.assertEqual(self.api.items[0]["fieldValueByName"]["optionId"],
                         self.config["project"]["options"]["Done"])
        self.assertIn("GitHub-Issue: " + self.api.issues[0]["url"], self.path.read_text(encoding="utf-8"))
        initial_state = (self.root / "state.json").read_bytes()
        initial_source = self.path.read_bytes()
        self.api.writes.clear()
        self.assertEqual(self.execute(), 0)
        self.assertEqual(self.api.writes, [])
        self.assertEqual((len(self.api.issues), len(self.api.items)), (1, 1))
        self.assertEqual((self.root / "state.json").read_bytes(), initial_state)
        self.assertEqual(self.path.read_bytes(), initial_source)

    def test_reopening_cancellation_and_supersession(self):
        self.assertEqual(self.execute(), 0)
        for status, state, reason, stage in (
            ("IN_PROGRESS", "OPEN", None, "In progress"),
            ("CANCELLED", "CLOSED", "NOT_PLANNED", "Backlog"),
            ("DONE", "CLOSED", "COMPLETED", "Done"),
            ("SUPERSEDED", "CLOSED", "NOT_PLANNED", "Backlog"),
        ):
            text = self.path.read_text(encoding="utf-8")
            text = sync.re.sub(r"^Status: .+$", "Status: " + status, text, flags=sync.re.M)
            self.path.write_text(text, encoding="utf-8")
            self.assertEqual(self.execute(), 0, self.output.getvalue())
            issue = self.api.issues[0]
            self.assertEqual((issue["state"], issue["stateReason"]), (state, reason))
            self.assertEqual(self.api.items[0]["fieldValueByName"]["optionId"],
                             self.config["project"]["options"][stage])

    def test_unrelated_body_survives_managed_updates(self):
        self.assertEqual(self.execute(), 0)
        self.api.issues[0]["body"] = "Human preface\n\n" + self.api.issues[0]["body"] + "\n\nHuman tail"
        self.change("Public fixture", "Public updated purpose")
        self.assertEqual(self.execute(), 0)
        body = self.api.issues[0]["body"]
        self.assertTrue(body.startswith("Human preface\n\n"))
        self.assertTrue(body.endswith("\n\nHuman tail"))
        self.assertIn("Public updated purpose", body)

    def test_managed_human_edits_block_but_independent_work_continues(self):
        self.assertEqual(self.execute(), 0)
        self.api.issues[0]["body"] = self.api.issues[0]["body"].replace("Public fixture", "Human edit")
        self.config["workstreams"]["wp-02"] = "02_SECOND.todo"
        (self.root / "TODO" / "02_SECOND.todo").write_text(self.initial, encoding="utf-8")
        sync.save_json(self.config_path, self.config)
        self.api.writes.clear()
        self.assertEqual(self.execute(), 1)
        self.assertEqual(len(self.api.issues), 2)
        self.assertIn("Human edit", self.api.issues[0]["body"])
        self.assertIn("Remote human edit", self.output.getvalue())

    def test_remote_lifecycle_and_board_edits_conflict(self):
        self.assertEqual(self.execute(), 0)
        self.api.issues[0]["state"] = "OPEN"
        self.api.writes.clear()
        self.assertEqual(self.execute(), 1)
        self.assertEqual(self.api.writes, [])
        self.api.issues[0]["state"] = "CLOSED"
        self.api.items[0]["fieldValueByName"]["optionId"] = "human-option"
        self.assertEqual(self.execute(), 1)
        self.assertEqual(self.api.writes, [])

    def test_partial_project_failure_retains_issue_then_recovers(self):
        self.api.fail_add = True
        self.assertEqual(self.execute(), 1)
        self.assertEqual(len(self.api.issues), 1)
        self.assertIn("adding_item", self.state()["workstreams"]["wp-01"])
        # A failed/uncertain add is not retried blindly. Explicit reconciliation
        # confirms this deterministic fixture failure created no item.
        state = self.state()
        state["workstreams"]["wp-01"].pop("adding_item")
        sync.save_json(self.root / "state.json", state)
        self.assertEqual(self.execute(), 0)
        self.assertEqual((len(self.api.issues), len(self.api.items)), (1, 1))

    def test_uncertain_create_recovers_marker_without_duplicate(self):
        self.api.timeout_create = True
        self.assertEqual(self.execute(), 1)
        self.assertIn("creating", self.state()["workstreams"]["wp-01"])
        self.assertEqual(self.execute(), 0, self.output.getvalue())
        self.assertEqual((len(self.api.issues), len(self.api.items)), (1, 1))

    def test_uncertain_add_recovers_visible_item_without_duplicate(self):
        add_item = self.api.add_item

        def uncertain_add(project, issue):
            add_item(project, issue)
            raise sync.SyncError("uncertain Project add")

        with patch.object(self.api, "add_item", side_effect=uncertain_add):
            self.assertEqual(self.execute(), 1)
        self.assertIn("adding_item", self.state()["workstreams"]["wp-01"])
        self.assertEqual(self.execute(), 0, self.output.getvalue())
        self.assertEqual((len(self.api.issues), len(self.api.items)), (1, 1))

    def test_uncertain_create_without_visible_issue_does_not_retry(self):
        work = sync.parse_workstream(self.path, "wp-01", self.config, self.root)
        state = {
            "schema": 1, "repository_id": self.config["repository_id"],
            "project_id": self.config["project"]["id"],
            "workstreams": {"wp-01": {"creating": {"title": work["title"], "managed": work["managed"]}}},
        }
        sync.save_json(self.root / "state.json", state)
        self.assertEqual(self.execute(), 1)
        self.assertEqual(self.api.writes, [])

    def test_backlink_failure_recovers_verified_remote_progress(self):
        with patch.object(sync, "write_backlink", side_effect=OSError("disk unavailable")):
            self.assertEqual(self.execute(), 1)
        self.assertIn("pending", self.state()["workstreams"]["wp-01"])
        self.api.writes.clear()
        self.assertEqual(self.execute(), 0)
        self.assertEqual(self.api.writes, [])
        self.assertEqual((len(self.api.issues), len(self.api.items)), (1, 1))

    def test_failed_inventory_is_not_an_empty_baseline(self):
        self.api.fail_inventory = True
        with self.assertRaisesRegex(sync.SyncError, "read failure"):
            self.execute()
        self.assertEqual(self.api.writes, [])
        self.assertFalse((self.root / "state.json").exists())

    def test_rename_keeps_identity_and_missing_file_never_deletes(self):
        self.assertEqual(self.execute(), 0)
        renamed = self.path.with_name("01_RENAMED.todo")
        self.path.rename(renamed)
        self.config["workstreams"]["wp-01"] = renamed.name
        sync.save_json(self.config_path, self.config)
        self.assertEqual(self.execute(), 0)
        self.assertEqual(len(self.api.issues), 1)
        renamed.unlink()
        self.api.writes.clear()
        self.assertEqual(self.execute(), 1)
        self.assertEqual(self.api.writes, [])

    def test_unknown_composite_status_and_missing_schema_fail_closed(self):
        self.change("Status: DONE", "Status: DONE locally; publication BLOCKED")
        self.assertEqual(self.execute(), 1)
        self.assertEqual(self.api.writes, [])
        self.path.write_text(self.initial.replace("Completion log:", "History:"), encoding="utf-8")
        self.assertEqual(self.execute(), 1)
        self.assertEqual(self.api.writes, [])

    def test_blocker_retains_stage_and_explicit_description(self):
        self.change("Status: DONE", "Status: BLOCKED")
        self.change("Fixture workstream", "Fixture workstream\nBlocked-stage: In review\nBlocker: Await owned fixture")
        self.assertEqual(self.execute(), 0)
        self.assertEqual(self.api.issues[0]["state"], "OPEN")
        self.assertIn("Blocker: Await owned fixture", self.api.issues[0]["body"])
        self.assertEqual(self.api.items[0]["fieldValueByName"]["optionId"],
                         self.config["project"]["options"]["In review"])

    def test_duplicate_markers_and_removed_membership_do_not_recreate(self):
        self.assertEqual(self.execute(), 0)
        original = copy.deepcopy(self.api.issues[0])
        self.api.issues.append({**original, "id": "duplicate"})
        self.api.writes.clear()
        self.assertEqual(self.execute(), 1)
        self.assertEqual(self.api.writes, [])
        self.api.issues.pop()
        self.api.items.clear()
        self.assertEqual(self.execute(), 1)
        self.assertEqual(self.api.writes, [])

    def test_backlink_conflict_and_public_projection(self):
        self.change("Public fixture", "Files: " + str(self.root) + "\\build\\example")
        self.assertEqual(self.execute(), 0)
        self.assertNotIn(str(self.root), self.api.issues[0]["body"])
        self.change(self.api.issues[0]["url"], "https://github.com/example/fixture/issues/999")
        self.api.writes.clear()
        self.assertEqual(self.execute(), 1)
        self.assertEqual(self.api.writes, [])

    def test_unmapped_local_backlink_blocks_create(self):
        self.change("Fixture workstream", "Fixture workstream\nGitHub-Issue: https://github.com/example/fixture/issues/7")
        self.assertEqual(self.execute(), 1)
        self.assertEqual(self.api.writes, [])

    def test_secret_shaped_content_blocked_without_echoing(self):
        dummy = "ghp_" + "A" * 36
        self.change("Public fixture", dummy)
        self.assertEqual(self.execute(), 1)
        self.assertNotIn(dummy, self.output.getvalue())
        self.assertEqual(self.api.writes, [])


class TransportTests(unittest.TestCase):
    def test_paginated_inventory_reads_every_page(self):
        pages = [
            {"repo": {"nodes": [1], "pageInfo": {"hasNextPage": True, "endCursor": "next"}}},
            {"repo": {"nodes": [2], "pageInfo": {"hasNextPage": False, "endCursor": None}}},
        ]
        api = sync.GitHub()
        with patch.object(api, "request", side_effect=pages) as request:
            self.assertEqual(api.connection("query", ("repo",)), [1, 2])
            self.assertEqual(request.call_args.kwargs["cursor"], "next")

    def test_mutation_timeout_is_never_retried(self):
        with patch.object(sync.subprocess, "run", side_effect=subprocess.TimeoutExpired("gh", 120)) as command:
            with self.assertRaisesRegex(sync.SyncError, "uncertain"):
                sync.GitHub().request("mutation { fixture }")
            self.assertEqual(command.call_count, 1)

    def test_failure_messages_redact_child_credential(self):
        dummy = "fixture-only-secret"
        result = subprocess.CompletedProcess("gh", 1, "", "failure " + dummy)
        with patch.dict(sync.os.environ, {"GH_TOKEN": dummy}), patch.object(sync.subprocess, "run", return_value=result):
            with self.assertRaises(sync.SyncError) as raised:
                sync.GitHub().request("query { fixture }")
            self.assertNotIn(dummy, str(raised.exception))


if __name__ == "__main__":
    unittest.main()
