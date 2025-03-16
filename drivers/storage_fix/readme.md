# Storage Drivers for ARM64 Non-DMA-Coherent Devices

The NVMe and AHCI drivers that come with Windows only work on devices with
cache-coherent DMA access. Some low-cost ARM64 devices (Raspberry Pi 5, NXP
i.MX 8M, Orange Pi 5) do not have cache-coherent DMA access. On these devices,
access to NVMe or AHCI storage will usually hang the device.

This package includes drivers that have been patched to work around this
limitation.

- storahci.sys fixes hangs on AHCI (SATA) storage.
- stornvme.sys fixes hangs on NVMe storage.

Drivers patched by Doug Cook (https://github.com/idigdoug).

Please report issues here:
[Rockchip-Windows-Drivers](https://github.com/worproject/Rockchip-Windows-Drivers/issues)

## Installation

To install this package, you must set the Windows installation to test-mode
(to allow test-signed drivers) and then you must overwrite the corresponding
driver in C:\Windows\System32\Drivers, e.g. using a system-file update tool
like sfpcopy.

## Known issues

These patches are not perfect and you may still encounter issues.

## storahci.sys: corrupt SMART data or crash in disk.sys

Even with this fix applied, AHCI drives may still return corrupt data for SMART
requests. This is very rare for the on-chip AHCI controller but happens quite
frequently for AHCI controllers in PCIe slots.

- On Windows builds 27765 or later, this results in invalid SMART results.

- On Windows builds before 27765, this will frequently result in a blue-screen
  crash in disk.sys.

The problem is caused by an issue in storport.sys that we cannot work around
in the storahci.sys driver.

There is no good workaround. The only way to avoid the crash is to use Windows
build 27765 or later or to not use PCIe-attached SATA controllers.

## stornvme.sys: unresponsive system or DPC Watchdog Violation

[Tracked with issue 60](https://github.com/worproject/Rockchip-Windows-Drivers/issues/60)

Even with this fix applied, high-performance NVMe SSDs may cause the system to
become temporarily unresponsive or may trigger a "DPC Watchdog Violation"
blue-screen crash when an NVMe controller is under heavly I/O load, e.g. during
prolonged benchmarks or stress testing.

We believe this problem occurs because the SSD works too quickly for the system
to keep up when operating in legacy PCIe interrupt mode. In this mode, all of the
data produced by the NVMe is processed by a single CPU. If the CPU is kept 100%
busy with NVMe data for too long, other tasks that the CPU is responsible for
handling will not get done. Eventually, Windows notices a problem and the system
crashes.

This is a complicated issue and we don't yet have a perfect fix for it. To find
the right solution, we need more information -- we need more people to try various
solutions to see which ones help and which ones don't work.

- If you find a solution that works, please share your experience in
  [github issue 60](https://github.com/worproject/Rockchip-Windows-Drivers/issues/60).
- If you find that even after trying the solutions below, you still have problems,
  please share details about your experience in
  [github issue 60](https://github.com/worproject/Rockchip-Windows-Drivers/issues/60),
  including the details about the hardware you're using, the situation where you
  have trouble, and what you've tried that didn't work.

If you are hitting this issue, please try the following:

- Install the registry values from `StorNvmeCoalescing.reg` (in this .zip file).
  Reboot. See if that resolves your problem. (If so, please report your experience
  in the above github issue.)
- Use regedit.exe to edit the `InterruptCoalescingEntry` and `InterruptCoalescingTime`
  registry values under `HKEY_LOCAL_MACHINE\SYSTEM\ControlSet001\Services\stornvme\Parameters\Device`.
  The value should be `REG_MULTI_SZ` with a value like `* 3`. Try numbers from 0 to 255.
  In theory, higher numbers are less likely to crash but might reduce system performance.
- If you are a driver expert, you might try making changes to the
  [Interrupt Affinity](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/interrupt-affinity-and-priority)
  settings for the stornvme driver to force interrupts to be serviced on the A76 cores.
  If you find settings that help, please describe them in the above github issue.

## Patches included in these drivers

### Fix DMA coherence issues

**storahci:**

- Clean cache for command-table before issuing IDENTIFY command.
- Invalidate cache for data buffer before issuing PRDT commands.
- Clean cache for command-table before normal commands.
- Allocate DMA buffers from uncached memory.

**stornvme:**

- Invalidate cache for data buffer before issuing commands.
- Clean cache for PrpList before issuing commands.
- Allocate DMA buffers from uncached memory.
- Remove the `STOR_FEATURE_DMA_ALLOCATION_NO_BOUNDARY` flag from
  FeatureSupport so we don't try to allocate small DMA buffers from pool.

### Fix crash dumps

In both storahci and stornvme, the driver encounters WHEA errors when running
in crash-dump mode due to using uncached memory as normal memory. This causes
problems because some devices do not support interlocked or unaligned
operations on uncached memory.

In storahci, the issue occurs with the channel extension.

In stornvme, the issue occurs with the submission queues, completion queues,
and command id context.

Old behavior:

- When running normally, the memory is allocated from non-paged pool. This
  works fine.

- When running in crash-dump mode, the first (and only) allocation of this
  memory is carved out of the adapter's NonCachedExtension.

Fix:

- Create a static (global) variable that will be used the first time we need
  this chunk of memory. This avoids an allocation when running normally and
  provides cached memory when running in crash-dump mode.

### Fix DPC Watchdog Violation on RK3588

On RK3588 devices, the device occasionally gets into a state where a
doorbell gets missed.

- Device has completed items up through item N.
- Driver has acknowledged (via doorbell) all items up through item N.
- Device thinks driver has acknowledged items up through item N-1.

This results in an infinite interrupt loop:

- Device raises an interrupt to notify the driver of item N.
- Driver has already acknowledged item N so it does nothing.
- Repeat forever.

To fix this, when 10 do-nothing interrupts occur in a row, queue a DPC that
rings the completion doorbell with the current value for all queues.

In addition, fix a race condition that may cause interrupts to become
unmasked when they should be masked.

## Changelog

- **fix8:** Add workaround for DPC Watchdog timeout on NVMe storage on RK3588
  systems. The NVMe driver will now ring all NVMe doorbells if 10 consecutive
  unexpected interrupts occur. Also fix an issue with interrupt masking.

- **fix7:** Broken. Do not use. (Adds a workaround for DPC Watchdog, but the
  released build was missing part of the fix.)

- **fix6:** Broken. Do not use. (Failed attempt at workaround for DPC Watchdog
  timeout on NVMe storage on RK3588 systems - stop processing DPC sooner.)

- **fix5:** Add support for collecting crash dumps during blue screens.

- **fix4:** First generally-useful release. Addresses most hangs but does not
  work for crash dumps.
