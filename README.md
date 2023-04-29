# Windows on Arm device drivers for Rockchip
This repository contains drivers for RK35xx-based platforms, with a focus on RK3588(S).

## Hardware support status
|Device|Driver|Status|Additional information|
| --- | --- | --- | --- |
|USB 3 Host|usbxhci (Inbox)|🟢 Working|USB 3.0-only due to hardware limitation, shared USB 2 is provided by dedicated controllers.|
|USB 3 Dual Role|usbxhci (Inbox)|🟡 Partially working|Host mode and USB 2.0-only, no dual role capability. Depends on USB/DP Alt Mode switching.|
|USB 2.0 & 1.1|usbehci (Inbox)|🔴 Not working|Windows bugchecks if enabled. USBOHCI driver for USB 1.1 is missing in ARM64 builds.|
|PCIe 3.0 & 2.1|pci (Inbox)|🔴 Not working|MSI & ITS silicon bugs, bad ACPI descriptors|
|SATA|mshdc (Inbox)|🔴 Not working|If enabled, drive enumerates with no IDs and hangs.|
|eMMC|[dwcsdhc](https://github.com/worproject/Rockchip-Windows-Drivers/tree/master/drivers/sd/dwcsdhc)|🟢 Working||
|SD/SDIO||🔴 Not working||
|CPU frequency scaling||🔴 Not working|Clocks limited at values set by UEFI.|
|HDMI output|MSBDD (Inbox)|🟢 Working|Single display with mode limited at 1080p 60 Hz, provided by UEFI GOP.|
|HDMI input||🔴 Not working||
|DisplayPort output||🔴 Not working||
|HDMI audio||🔴 Not working||
|DisplayPort audio||🔴 Not working||
|Analog audio||🔴 Not working||
|Digital audio||🔴 Not working||
|USB/DP Alt Mode||🔴 Not working||
|GPU||🔴 Not working|Software-rendered|
|NPU||🔴 Not working||
|Multimedia codecs||🔴 Not working||
|DSI||🔴 Not working||
|CSI||🔴 Not working||
|GMAC Ethernet||🔴 Not working||
|UART||🔴 Not working|No OS driver but debugging does work on UART2, being configured by UEFI.|
|GPIO||🔴 Not working||
|I2C||🔴 Not working||
|I2S||🔴 Not working||
|SPI||🔴 Not working||
|CAN bus||🔴 Not working||
|SPDIF||🔴 Not working||
|SARADC||🔴 Not working||
|PWM||🔴 Not working||

## Building
1. Install a recent version of Visual Studio, Windows SDK and WDK.
2. Clone this repo.
3. Open the `Rockchip-Windows-Drivers\build\RockchipDrivers.sln` solution in Visual Studio.
4. Set the desired build configuraton (Release or Debug).
5. Build -> Build Solution (or <kbd>Ctrl+Shift+B</kbd>).

The resulting driver binaries will be located in the `Rockchip-Windows-Drivers\build\ARM64\Debug\Output` (or Release) directory.

## Firmware
The UEFI + ACPI implementation needed to run Windows and these drivers is available at <https://github.com/edk2-porting/edk2-rk35xx>.

## Credits
**Driver signing provided by [Theo](https://github.com/td512) @ [CHASE®](https://chase.net.nz/)**
