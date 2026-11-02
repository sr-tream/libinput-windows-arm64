# Installation and recovery

## Requirements

The validated setup is an ASUS Vivobook S 15 S5507QA/S5507QAD with an ASCE1200 Precision Touchpad, Windows 11 build 26200, and native ARM64 execution. The existing ASUS HID/I2C driver must remain installed.

The worker validates VID `04F3`, PID `32E2`, a 42-byte input report and the expected contact descriptor. It also requires the touchpad settings API used by `SPI_GETTOUCHPADPARAMETERS` and `SPI_SETTOUCHPADPARAMETERS`. A second touchpad makes global panning replacement ambiguous and prevents startup. Other hardware needs implementation and validation work.

## Build

Install these dependencies before running [build.ps1](../build.ps1):

1. Visual Studio C++ tools with the MSVC ARM64 include files and libraries. The script locates the installation through `vswhere.exe`.
2. A Windows SDK containing ARM64 headers, libraries and the resource compiler. A recent SDK exposing the touchpad settings API is required.
3. LLVM with `clang-cl.exe` at `C:\Program Files\LLVM\bin\clang-cl.exe`.

```powershell
.\build.ps1
```

The executable is written to the repository root. Intermediate object and resource files go to `build/`. C++ runtime linkage is static. No libinput download or Linux environment is needed during the build.

## First run

Keep an external mouse available during the initial hardware test. Place the folder in a location you control, and keep its path stable once login startup is configured.

```powershell
# Only create this file if you do not already have preferences.
if (!(Test-Path .\gestures.ini)) {
    Copy-Item .\gestures.example.ini .\gestures.ini
}
.\libinput-scroll.exe --supervise
```

The supervisor requests UAC and launches the worker in the interactive user session. Elevation allows wheel injection into applications running as administrator. Process priority remains Normal.

Use `Settings.cmd`, double-click the tray icon, or run the EXE without arguments to open settings. The settings window is a separate process; it does not start the worker by itself.

## Login startup

```powershell
.\Enable-Autostart.ps1 -StartNow
```

The installer requests UAC and registers `\LibinputASCE1200Scroll` for the current interactive user. It runs `libinput-scroll.exe --supervise` with highest privileges, without storing a password. Battery operation is allowed and there is no execution time limit. `-StartNow` also starts the task immediately.

The generated `autostart-task.xml` contains the local account and installation path; it is ignored by Git. If the same task name points to a different installation, the installer refuses to replace it. Disable startup from the old installation before moving to another folder, then enable it from the new folder.

## Stop, restart and remove

Stop and restore the previous native panning state using **Ctrl+Alt+F12**, the tray menu, or:

```powershell
.\Stop.cmd
```

`Restart.ps1` stops the worker, waits for the existing scheduled task to exit and restarts it. Without an accessible task, it falls back to the supervisor launcher and may request UAC.

To remove login startup and stop the current session:

```powershell
.\Disable-Autostart.ps1
.\Stop.cmd
```

Disabling startup alone leaves the current session running. After stopping, the installation folder can be removed. No driver removal or registry-backup import is needed.

## Recovery behavior

The worker captures the original system panning value before changing it. Changes apply to the current session and are not persisted in the Windows user profile. A separate watchdog waits for the worker and restores panning after an unexpected exit. Normal cleanup restores panning and releases the watchdog.

If scrolling becomes unusable, use the external mouse and `Stop.cmd`. Windows touchpad settings provide an independent way to inspect the native scroll configuration if both the worker and its watchdog were terminated.

Keep the complete folder until all worker, supervisor, watchdog and settings processes have exited; Windows can keep a running executable locked.
