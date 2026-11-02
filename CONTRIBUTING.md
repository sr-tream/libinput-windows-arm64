# Contributing

Use English for issues, documentation, comments and user-facing strings. The application UI is English by default.

For a bug report, include the Windows build, laptop/touchpad model, selected mode and target application. Describe the exact gesture and expected behavior. Device support is currently limited to the validated ASCE1200 setup; reports from other devices should include descriptor information rather than only a VID/PID substitution.

Before sending a change:

1. Build with `build.ps1` and run `Test.ps1`; use `-Ui` for settings changes.
2. Keep input-path work bounded. Do not add blocking provider queries or synchronous mouse hooks.
3. Preserve click, fresh-touch and window-change cancellation, as well as stop/watchdog recovery.
4. Document behavior changes and the limitations of any hardware checks performed.

Tests should exercise behavior and failure cases without registering the real touchpad or injecting real input. Keep raw personal recordings, generated executables, machine-specific startup XML, driver backups and credentials out of commits.

The project uses the [MIT license](LICENSE). Preserve the original upstream notices in `third_party/libinput/` and record any updated upstream version in `UPSTREAM.txt` and `NOTICE.md`.
