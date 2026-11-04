# Test plan

Automated tests are local-only and must not call real WMI activation methods.
They cover key parsing, OS classification, licensing-record selection, status
mapping, error mapping, workflow state transitions, and bounded IPC parsing as
those modules are introduced.

CI builds x64 Release with MSVC and runs CTest. It must never contain or install
a real key or alter services, scheduled tasks, registry values, licensing files,
or activation configuration.

Manual activation tests require an owned or explicitly authorized Windows 10 or
Windows 11 x64 machine and a legitimate key or entitlement. Keep keys and device
identifiers out of screenshots and test artifacts. Verify normal-user status
reading, UAC cancellation, invalid/wrong-edition handling, offline behavior,
Windows Settings agreement, high DPI, keyboard access, and clean shutdown.

Company KMS automated coverage includes hostname/port validation, optional
server preservation, exact edition setup-key selection, installation and
configuration failure, existing KMS renewal, preservation of activated retail
licenses, ambiguity rejection, provider success without activation, and expiry
derived from Windows data. Protocol tests cover malformed lengths, headers,
commands, unterminated input, hidden trailing data, response bounds and errors.

Manual company acceptance (not executed by the build):

- Use an authorized Windows 10/11 Pro VM and an IT-provided KMS host.
- Verify automatic discovery on the company network and explicit host/port.
- Cancel UAC: no request sent; then check same-account and alternate-admin UAC.
- Activate an unactivated Pro installation and compare with Windows Settings.
- Renew an existing KMS license and check Windows-reported remaining time.
- Check offline/VPN/DNS failures and exact errors; no false success message.
- Check installed setup key / server after failures; changes are not atomic.
- Verify working retail/OEM/MAK activation is never replaced by the KMS action.
- Verify Home/Server/ambiguous edition records refuse the modifying action.
- Test missing/swapped helper, wrong pipe peer, shutdown, double clicks and timeout.
- Verify unsigned builds are never presented as a verified publisher.

