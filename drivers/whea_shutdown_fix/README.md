# Occasional WHEA crash during Windows shutdown

Sometimes during system shutdown, the system will crash with a WHEA memory
error.

Issue partially understood:

- Windows is trying to read from the ACPI FACS table.
- For some reason, this triggers a memory read abort. (Exact root cause
  unknown.)
- The same issue has been reported on other boards, so it's not specific to
  RK3588.

Workaround:

- Add a DWORD registry value PowerSimulateHiberBugcheck set to 0x40 to registry
  key `HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Session Manager`.

You can use the `PowerSimulateHiberBugcheck.reg` file to do this.
