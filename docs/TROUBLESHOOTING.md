# Known limitations and troubleshooting

| Symptom | Explanation or action |
| --- | --- |
| The EXE opens Settings but does not scroll | Run `--supervise` or enable startup. Opening Settings alone does not start the worker. |
| Startup reports unsupported settings or device | Confirm the validated hardware and touchpad API support. The device guard intentionally rejects unknown descriptors. |
| Another touchpad is present | Global panning replacement is ambiguous; use a supported single-touchpad setup. A regular external mouse is fine. |
| Wheel output fails in administrator windows | Use the elevated supervisor or highest-privilege login task. `CONFIG` should contain `elevated=1`. |
| A browser back/forward swipe is missed | Windows driver mode makes an early, immutable direction choice. Lift every finger between attempts. A vertical-looking start may stay with the wheel route. |
| Touchscreen mode does not navigate back in Firefox | The application decides what screen gestures mean. Touchscreen emulation alone does not provide Firefox's native touchpad navigation semantics. |
| Screen gestures also move vertically | Enable **Lock horizontal axis**. Once chosen, a screen contact owns its subsequent motion until release. |
| A screen drag reaches the edge and stops | The synthetic contact is clamped to the original client bounds. Infinite dragging and independent touchscreen momentum are not implemented. |
| The mouse cursor moves or disappears during a screen gesture | Windows can promote touch to mouse input and change cursor visibility/position. The utility does not reposition it afterward. |
| Some applications scroll in coarse steps | Applications interpret wheel units and momentum differently. This backend sends wheel input rather than a synthetic Precision Touchpad stream. |
| Scroll direction or speed feels wrong | Adjust `settings.ini`, then restart. Touchscreen sensitivity/reversal are separate profile settings. |
| An application rule no longer matches | Check its executable path. Versioned package paths can change after updates. Basename rules are also supported. |
| Startup installer refuses an existing task | Disable startup from the installation that currently owns the task before registering another path. |
| An EXE cannot be replaced | Stop the worker/supervisor and close Settings; keep the watchdog available until native panning is restored. |
| Adaptive cursor stops and native movement returns | Check `pointer.log` and `POINTER_WORKER` in `session.log`. An error or Ctrl+Alt+F11 leaves the helper stopped. Toggle the pointer option off/on with Apply, or restart the service. |
| Mouse adaptive speed feels wrong | Match the configured DPI to the mouse's hardware setting. One DPI value serves all mice. Disable the mouse option to retain Windows mouse processing. |
| Tablet or absolute mouse stops pointer replacement | Absolute mouse devices are unsupported. The helper exits and restores native movement. Leave pointer acceleration disabled for this setup. |

## Responsiveness

The scrolling worker has no synchronous mouse hook or UI Automation provider in its input path. Optional pointer acceleration uses a minimal motion hook on a dedicated thread in a separate process. Process priority is Normal. A previous bounded Realtime experiment did not improve the reported reversal feel, so Realtime is not enabled.

Application-side wheel smoothing can still create different results. The project does not apply browser-specific horizontal speed compensation. Compare a native scroll session after using `Stop.cmd` when investigating application behavior.

## Emergency stop

Use **Ctrl+Alt+F12**, `Stop.cmd` or the tray stop command. Normal exit restores the previously captured native panning flag. The watchdog covers worker termination, but terminating both processes together can defeat that recovery; use the external mouse and Windows touchpad settings if necessary.

**Ctrl+Alt+F11** restores native cursor movement while the scrolling worker continues. Pointer preferences are saved, so the helper can start again on the next service launch. Raw Input consumers, exclusive-input games, remote sessions and absolute pointing devices have not been validated with pointer replacement; stop it when testing those paths.

Stopping the app leaves the login task enabled. To stop future login launches, also run `Disable-Autostart.ps1`.

## Reporting a problem

Include the Windows build, laptop model, touchpad VID/PID, app name, selected gesture mode and a precise reproduction. Describe whether the issue occurs during contact, reversal, release, click or switching windows. Review logs before attaching them and omit local account IDs, personal paths and raw recordings unless needed.
