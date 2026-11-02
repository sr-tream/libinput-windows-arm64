# Configuration

## Application profiles

Open **Settings** and select **Default**, or add an application from the list of running programs or by browsing to its EXE. The picker selects an executable; it does not launch it.

| Mode | INI key | Gesture routing |
| --- | --- | --- |
| Windows driver | `v1` | An early horizontal gesture goes to the native Windows handler. An early vertical choice stays with wheel scrolling until every finger lifts. |
| Touchscreen | `touchscreen` | A sufficiently aligned horizontal gesture becomes one synthetic screen contact. A short initial vertical drift can still become a screen gesture. |
| No gestures | `none` | Both scroll axes use wheel output and the utility's own momentum. |

The `v1` key remains the storage identifier for **Windows driver**. The shipped example uses Windows driver by default, Windows driver for `firefox.exe`, and Touchscreen for `Telegram.exe`.

Rules match executable paths without case sensitivity. An exact full path takes precedence over a basename rule such as `firefox.exe`. Unmatched applications use Default. A path containing an application version may need updating after the application moves or updates.

The settings UI saves `gestures.ini` atomically. The background worker reloads it, and a gesture latches its profile until every finger lifts. Saving a new profile does not change an active gesture. Up to 128 application rules are supported.

### Touchscreen options

| Setting | Range/default | Meaning |
| --- | --- | --- |
| Sensitivity | 1–40 px/mm; default 10 | Converts touchpad centroid travel to screen-contact movement. |
| Invert horizontal direction | Off | Reverses the screen contact's horizontal motion. |
| Lock horizontal axis | Off | Holds the screen contact's vertical coordinate fixed. |

Ordinary vertical scrolling uses the wheel backend. Once a horizontal gesture has become a screen contact, it remains captured until release and may include vertical motion unless axis lock is enabled. Touchscreen gestures use the target application's drag handling and inertia. The utility does not add its own touchscreen momentum.

### INI example

```ini
[default]
mode=v1
touch_pixels_per_mm=10
touch_invert_x=0
touch_lock_y=0

[applications]
count=1

[app.0]
image=Telegram.exe
mode=touchscreen
touch_pixels_per_mm=10
touch_invert_x=0
touch_lock_y=1
```

The UI writes UTF-16LE so Unicode executable paths round-trip correctly. Prefer using the UI for editing paths.

## Wheel scrolling and momentum

`settings.ini` is read when the worker starts. Restart through `Restart.ps1` after changing it.

```ini
[scroll]
scale=3.5
reverse_native_direction=1
inertia=1
inertia_friction=0.002
```

| Parameter | Meaning |
| --- | --- |
| `scale` | Direct wheel and momentum scale; clamped to 0.1–10. |
| `reverse_native_direction` | Reverses both wheel axes relative to the current Windows touchpad direction preference. |
| `inertia` | Enables the utility's wheel momentum when set to 1. |
| `inertia_friction` | Exponential decay parameter; lower positive values give a longer coast. |

Velocity decays as `v(t + dt) = v(t) * (1 - friction)^dt_ms`. At the default `0.002`, the decay time constant is approximately half a second. Application interpretation of wheel events still affects the result.

Wheel momentum stops on a real mouse button or touchpad tap, a new touch with two or more fingers, a foreground-window change, or a changed wheel destination. Moving the cursor within the same destination preserves it. Switching applications cannot transfer an old wheel coast into the new window.

## Adaptive pointer acceleration

Choose **Pointer acceleration…** in Settings. **Use settings** updates the pending values; **Apply** in the main window saves them. These options are global, independent of application gesture rules. Pointer changes restart the separate pointer helper immediately; gesture rules still wait for the next gesture.

| Option | Range/default | Meaning |
| --- | --- | --- |
| Enable adaptive pointer acceleration | Off | Replaces single-finger touchpad cursor movement with libinput's adaptive touchpad profile. |
| Touchpad speed | −1 to +1; default 0 | libinput's acceleration speed setting; 0 is the upstream baseline. |
| Also accelerate the external mouse | Off | Uses the adaptive mouse profile for physical relative mice. Otherwise their original relative counts are replayed through Windows mouse processing. |
| Mouse speed | −1 to +1; default 0 | Independent speed for the adaptive mouse profile. |
| Mouse DPI | 100–64000; default 1000 | Match the DPI selected on the mouse. One value applies to all connected relative mice. |

```ini
[pointer]
enabled=0
touchpad_speed=0
mouse_adaptive=0
mouse_speed=0
mouse_dpi=1000
```

The helper uses raw touchpad contacts and mouse counts, with absolute cursor output for adaptive motion. Windows pointer acceleration is not applied a second time to that output. It does not modify the Windows pointer-speed or Enhance Pointer Precision preferences. Clicks, wheel input and injected touchscreen gestures keep their existing processing.

Press **Ctrl+Alt+F11** to stop pointer replacement for the current session while scrolling continues. To resume, disable and apply the pointer option, then enable and apply it again, or restart the service. **Ctrl+Alt+F12** and `Stop.cmd` stop both scrolling and pointer replacement. The saved enable preference remains in effect for the next service start.
