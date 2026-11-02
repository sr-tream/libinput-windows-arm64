# Third-party notices

## libinput

The files in `third_party/libinput/` contain unchanged function extracts from libinput **1.32.0**, upstream commit `80f58e741e68f346a98b562eb06b44401c47938f`.

Upstream: <https://gitlab.freedesktop.org/libinput/libinput>

| Included file | Upstream location |
| --- | --- |
| `constant-filter.inc` | `touchpad_constant_filter` in `src/filter-touchpad.c` |
| `direction.inc` | `xy_get_direction` in `src/libinput-private.h` |
| `same-directions.inc` | `tp_gesture_same_directions` in `src/evdev-mt-touchpad-gestures.c` |
| `scroll-constraints.inc` | `tp_gesture_apply_scroll_constraints` in `src/evdev-mt-touchpad-gestures.c` |
| `pointer-trackers.inc` | `trackers_reset`, `trackers_feed`, `trackers_by_offset`, `calculate_trackers_velocity`, `trackers_velocity_after_timeout`, `trackers_velocity`, `calculate_acceleration_simpsons` in `src/filter.c` |
| `pointer-touchpad.inc` | `speed_factor`, `touchpad_accelerator_set_speed`, `touchpad_accel_profile_linear` in `src/filter-touchpad.c` |
| `pointer-mouse.inc` | `accelerator_set_speed`, `pointer_accel_profile_linear` in `src/filter-mouse.c` |
| `pointer-low-dpi.inc` | `pointer_accel_profile_linear_low_dpi` in `src/filter-low-dpi.c` |

The original copyright and permission notices remain in each extract. The upstream license is reproduced in [third_party/libinput/COPYING](third_party/libinput/COPYING). Compatibility types in `libinput_scroll.h` and `pointer_acceleration.h` allow these seventeen functions to run without Linux event devices or the full libinput runtime.

This project is an independent partial port. It is not maintained or endorsed by the libinput project, ASUS, Microsoft, Mozilla or Telegram.

## Other assets and implementations

The Windows adapter, settings UI, gesture routing and momentum implementation are covered by the root [MIT license](LICENSE). The application icon was generated with OpenAI's image generation tool; its generation prompt is retained in [assets/icon-prompt.txt](assets/icon-prompt.txt).

The momentum implementation uses exponential velocity decay with a configurable friction parameter. Its behavior is inspired by Firefox desktop APZ, but the event units and release-velocity estimator are specific to this project.

Windows SDK and Visual Studio components are build dependencies. Hardware vendor drivers are installed separately and are not distributed in this repository.
