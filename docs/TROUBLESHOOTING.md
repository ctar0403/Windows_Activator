# Troubleshooting

## Build prerequisites

Use Visual Studio 2022 or newer with **Desktop development with C++**, a Windows
10/11 SDK, and CMake 3.24+. Configure the x64 preset from a Visual Studio
Developer PowerShell.

## Activation help

WinActivate Manager never circumvents Windows licensing. For entitlement,
purchase, account, or hardware-change issues, open **Settings > System >
Activation** and use Microsoft's activation troubleshooter. Microsoft support
resources:

- <https://support.microsoft.com/windows/activate-windows-c39005d4-95ee-b91e-b399-2820fda32227>
- <https://support.microsoft.com/windows/get-help-with-windows-activation-errors-09d8fb64-ab7c-4b75-8b1d-0d1efb40bfa7>

Do not post a full product key in an issue, screenshot, log, or support message.

## Company KMS

Keep both EXEs together. Open the GUI normally; only activation requests UAC.
Connect to your workplace network/VPN. For automatic configuration keep the
server blank and port 1688. For a server supplied by IT, enter only its hostname
or IPv4 address (no URL scheme or appended port), with the port in its own box.
IPv6 literals and internationalized names are not accepted in this version.

- `0xC004F074` / `0x8007232B`: check network, VPN, DNS and company host with IT.
- `0xC004F038`: the KMS host has not reached its required activation count; IT
  must check the host. Windows clients normally require 25 qualifying computers.
- `0xC004F050`: confirm the installed edition supports the public setup key.
- Missing helper: rebuild and copy both EXEs together.
- No deterministic matching record: use Windows Activation Settings and contact
  IT. The app does not guess between edition records or upgrade Home to Pro.
- Timeouts/failures: refresh first. A key install or server change may already
  have completed even if activation failed. The app does not silently roll back.
- Zero or unavailable validity: the app reports the WMI value as-is; it does not
  invent an expiry date. Compare with Windows Activation Settings.

Diagnostics may display your internal KMS hostname. Redact company network
details before sharing screenshots publicly.

