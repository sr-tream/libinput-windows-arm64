# Development and testing

## Build layout

The application is C++20, compiled with `clang-cl` and linked against ARM64 MSVC/Windows SDK libraries. See [installation](INSTALLATION.md) for toolchain requirements. From the repository root:

```powershell
.\build.ps1
```

`build/` holds intermediates. The application and test executables are produced in the root and ignored by Git. The build embeds [app.manifest](../app.manifest), Common Controls v6, per-monitor DPI awareness, and the icon in [app.rc](../app.rc).

Build a single test with:

```powershell
.\build.ps1 -Name interaction-tests -Source interaction_tests.cpp
.\interaction-tests.exe
```

The build script selects the newest installed MSVC and SDK directories. Missing ARM64 libraries or touchpad API declarations should be fixed by installing the appropriate C++/SDK components. Changing the compiler location requires editing `build.ps1`.

## Automated checks

Run the source checks from a normal interactive desktop session:

```powershell
.\Test.ps1
# Also exercise the real settings UI against a separate fixture INI:
.\Test.ps1 -Ui
```

| Test | Coverage |
| --- | --- |
| `scroll_tests.cpp` | Scroll filtering, axis constraints, direction changes, wheel accumulation and momentum. |
| `gesture_handoff_tests.cpp` | Early native handoff, stable ownership, lift/restart, pinch, click and contact ordering. |
| `app_profiles_tests.cpp` | Unicode paths, exact/basename precedence, atomic save, hot reload and actual frame routing with simulated output. |
| `touchscreen_output_tests.cpp` | Screen-contact lifecycle, coordinate bounds, settings, echo filtering and cancellation. |
| `interaction_tests.cpp` | Actual click/focus callback paths and wheel-momentum cancellation. |

These checks do not register the real touchpad, inject real input or change its settings. The profile fixture creates its own INI and user-scoped notification event. The optional UI test briefly opens its own settings/picker windows and saves captures in `build/test-data/`; it uses a test INI and does not change personal application rules. An interactive desktop with visible applications is needed for the picker test.

## Optional replay

Personal input recordings are not included. An existing converted contact trace can be supplied to the scroll and routing checks:

```powershell
.\Test.ps1 -Trace C:\path\to\contacts.txt
```

Each line contains a timestamp in microseconds, a contact count, then `id x_mm y_mm` for each contact. A zero-contact line represents full release. The replay checks expect more than 10,000 reports and exercise real recorded scroll output and ownership stability.

During local validation, 26,283 reports were replayed. Before removal of the retired routing branches, 2.4 million generated routing decisions were compared with the previous production modes and matched exactly. Those figures describe completed local checks, not a hardware compatibility guarantee.

## Hardware validation

Synthetic tests cannot establish the final feel of application scrolling. For a hardware change, verify vertical and horizontal movement, direction reversals, release momentum, click/tap cancellation, Alt+Tab, administrator windows, stop recovery and the intended application's native/touchscreen gestures. Use an external mouse for the first live test.

Device support currently assumes five contact collections, 42-byte reports and the validated physical coordinate range. Changing only VID/PID checks is insufficient to add another touchpad. Extend descriptor validation, coordinate conversion and replay fixtures together.

## Diagnostics

The normal worker writes `session.log` with configuration, device fields, wheel samples and aggregated counters. `--profile` adds handler, message-age estimate and injection-time histograms. These are software timing measurements; they do not measure physical touch-to-display latency.

An explicit non-running probe such as `--seconds 30 --log probe.log` records raw contact fields and coordinates without injecting input. Treat that output as personal diagnostic data and review it before sharing. Normal logs may contain process IDs, motion deltas and pointer handles too.

`--test-recovery` deliberately exits a live settings-changing process to exercise the watchdog. It is an advanced hardware recovery test and is intentionally excluded from `Test.ps1`.
