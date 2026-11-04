# WinActivate Manager

Native Windows 10/11 x64 desktop app for company KMS activation and renewal.
Members do not need to enter a personal product key. The app uses documented
Windows licensing APIs and your organization's activation infrastructure.

## Use

1. Keep `WinActivate.exe` and `WinActivateHelper.exe` in the same folder.
2. Connect to your company network or VPN and open `WinActivate.exe` normally.
3. Leave the company server blank to use current Windows KMS configuration or
   DNS discovery. Alternatively enter your IT team's hostname and port (default
   `1688`). An entered host updates that Windows product's KMS configuration.
4. Click **Activate / renew with KMS** and approve the administrator prompt.
5. Check the freshly read status and **KMS validity**. Diagnostics shows the
   configured/discovered server, remaining time and exact Windows error codes.

For an unactivated supported edition, the app installs Microsoft's public KMS
client setup key for the existing edition when needed. It preserves working
retail/OEM/MAK activation and can renew an already activated KMS client.

KMS activation is normally valid for 180 days and Windows requests renewal
automatically. The app displays the remaining time reported by Windows rather
than promising a fixed 180 days. The company supplies the volume licensing
entitlement; a public setup key alone is not a license. See
[Microsoft's KMS documentation](https://learn.microsoft.com/windows-server/get-started/activation-troubleshoot-kms-general)
and [public client setup keys](https://learn.microsoft.com/windows-server/get-started/kms-client-activation-keys).
No public KMS server is preconfigured by this app.

## Supported systems

- Windows 10 or Windows 11 client/workstation, x64
- Automatic setup-key installation: Pro / N, Pro for Workstations / N,
  Education / N, Enterprise / N (non-LTSC)
- Home cannot use KMS; the app does not upgrade editions
- Windows Server, Windows 8/8.1, and x86 are not supported
- Visual Studio 2022 or newer with Desktop development with C++
- Windows 10 or Windows 11 SDK
- CMake 3.24 or newer

Activation does not provide security updates; servicing availability depends on the
edition and any applicable Microsoft program.

## Build and test

The easiest option is to double-click `build.cmd`. It configures an x64 Release
build, compiles the application, runs the mock-backed tests, and copies the
executables to `dist\WinActivate.exe` and `dist\WinActivateHelper.exe`. Keep both
files together. The script prefers the required
Visual Studio/CMake toolchain. If only MinGW is installed, it can create a local
test build, but that fallback is not a release-validation build.

From a terminal, the equivalent convenient command is:

```bat
build.cmd Release --no-pause
```

Use `build.cmd Debug --no-pause` for a Debug build.

From a Visual Studio Developer PowerShell:

```powershell
cmake --preset vs2022-x64
cmake --build --preset release
ctest --preset release
```

The CMake Release application is written beneath
`build/vs2022-x64/Release`. `build.cmd` additionally copies it to `dist`.

The GUI runs without administrator privileges. Clicking an activation button
starts the separate elevated helper using authenticated local IPC. **Activate
existing license** uses the installed entitlement without changing KMS settings.
Personal product-key input has been replaced by company KMS controls.

Automated tests use fake licensing backends and local pipe transport. They never
install a key or request activation. Live company KMS, UAC, high DPI and agreement
with Windows Settings require manual testing on an authorized device.

## Safety and privacy

The application has no telemetry, backend, account login, scheduled task, or
silent shell commands. The GUI independently rereads Windows status after each
operation, including failures. Key or server changes made by Windows can persist
even if activation subsequently fails; refresh and contact IT before retrying.

Local builds are **unsigned** unless your company signs both executables. For
company distribution, IT should sign both files with its trusted publisher
certificate. The app does not certify KMS server operators or license entitlement
and does not label unsigned builds as Microsoft-verified. See
[Security](docs/SECURITY.md) and [Troubleshooting](docs/TROUBLESHOOTING.md).

Real activation testing must be performed only on an owned or explicitly
authorized Windows device using a legitimate license. Automated tests never
install a key or change host licensing state.

