WindowPane - Windows x64 portable preview
A little window into any window.

Extract the ZIP, then open WindowPane.exe. No installer or administrator access
is required. The executable is unsigned and may trigger a Windows warning.

Open the source application, click "Snip a window region", and select its area.
Drag the upper-right dotted grip to move; right-click it for options.
The grip stays interactive even with transparent or click-through content.
Use controls for opacity, picked-color removal, resizing and update rate.

Ctrl+Alt+N: Snip again.
Ctrl+Alt+W: Hide/show.
Ctrl+Alt+O: Open controls.
Escape: Cancel selection or picking.
Closing controls hides them to the tray; use "Exit WindowPane" to quit.

Default target is 60 fps; alternatives are 1, 5, 10 and 30 fps. Actual cadence
depends on the source application and Windows capture/timer scheduling.
One pane, session-only settings, no auto-start. The source must stay open and
rendering; WindowPane does not refresh website data or preserve inactive tabs.
Source resize/closure requires a new snip. Matching foreground colors can
disappear when removing a background. Frosted-look grip is tint, not real blur.

Windows x64; capture API baseline is Windows 10 version 1903.
Broad app, HDR, mixed-DPI/multi-monitor and clean-machine testing are pending.
Hosted CI runs headless pixel tests, not the native capture/UI test suite.
No sign-in, network client, browser integration or ordinary frame recording.

Guide and source: https://github.com/Zanark/Windowpane
