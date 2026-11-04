# Security

## Trust boundary

WinActivate Manager is designed to run its GUI as the signed-in user. Read-only
licensing checks do not require elevation. A separate one-shot helper requests
administrator approval only after an explicit activation action. The helper
handles one request and exits; it does not install a service or scheduled task.

The GUI creates a GUID-named local message pipe with a restricted DACL and
`PIPE_REJECT_REMOTE_CLIENTS`. It checks the connected helper PID against the
process handle returned by `ShellExecuteExW`, and the sibling executable path.
The helper holds a handle to the GUI process and checks its executable path,
session, and the pipe server PID before accepting a request. Alternate-account
administrator elevation is allowed by the pipe ACL. Pipe I/O is bounded and
timed out; unknown commands, lengths, versions and malformed input are rejected.
The elevation request runs on a worker and does not block the UI thread.

The protocol carries KMS host/port or existing-license activation, never a
personal product key or general command line. Edition-specific public Microsoft
KMS setup keys are selected inside the helper. An entered server is applied to
the selected Windows product; blank input preserves configuration.

The project must never:

- generate, guess, collect, or persist product keys;
- pass a full key through command-line arguments or environment variables;
- write a key to files, logs, telemetry, crash reports, the registry, or an IPC
  endpoint name;
- embed third-party KMS services, patch licensing components, or use rearm
  as an activation bypass;
- declare activation successful without rereading the applicable Windows
  licensing record and observing `LicenseStatus == 1`.

Sensitive mutable buffers are cleared with `SecureZeroMemory` where practical.
This reduces accidental retention but cannot promise erasure of every copy made
by Windows, the C++ runtime, or the WMI provider.

Only a fresh applicable `LicenseStatus == 1` means Windows currently reports
activation. The GUI independently rereads the snapshot after the helper finishes,
including when the operation fails. A successful API call is not proof of
entitlement or server operator trust.

A timeout does not roll back changes already made by Windows. A failed server
configuration can partially change host/port; a setup-key install may remain
after an activation failure. Refresh before retrying and contact IT.

Unsigned builds do not establish publisher identity. Code-sign both executables
for distribution. Deploy together to an IT-managed directory whose permissions
prevent unauthorized replacement; peer path/PID checks do not substitute for
securing executable files or for a full security audit.

## Reporting a vulnerability

Do not include product keys, machine identifiers, account data, or diagnostic
dumps containing private information in a report. Share a minimal reproduction
with the project owner through a private channel established for this repository.

