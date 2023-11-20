# Windows on Arm device drivers for Rockchip
This repository contains drivers for RK35xx-based platforms, with a focus on RK3588(S).

## Hardware support status
|Device|Driver|Status|Additional information|
| --- | --- | --- | --- |
|USB 3 Host ports #1 and #2|usbxhci (Inbox)|🟢 Working|The "full" USB-3 ports (USB3OTG_0, USB3OTG_1) work correctly. Note that RK3588 devices (e.g. Opi5+) have 2 "full" USB-3 ports, while RK3588s devices (e.g. Opi5, Opi5B) have only 1 "full" USB-3 port.|
|USB 3 Host port #3|usbxhci (Inbox)|🟡 Partially working|USB-3 only, won't support USB-2 or USB-1 devices (even if you use a hub).<br> One of the USB-3 ports works by combining a USB3-only xHCI port with a USB2 EHCI+OHCI port. Since EHCI+OCHI aren't working (#4), this port will work for USB-3 devices but not for USB2 or USB1 devices.|
|USB 3 Dual Role|usbxhci (Inbox)|🟡 Partially working|Host mode, no dual role capability. Depends on USB/DP Alt Mode switching.<br> USB 3.0 only works in one orientation of the Type-C connector.|
|USB 2.0 & 1.1|usbehci (Inbox)|🔴 Not working|Windows bugchecks if EHCI device enabled (#4). USBOHCI driver for USB 1.1 is missing in ARM64 builds (#5).|
|PCIe 3.0 & 2.1|pci (Inbox)|🟡 Partially working|NVMe SSDs do not work with in-box storport.sys (#6, workaround available). Other PCIe devices (non-SSD) do work ok if drivers are available for them.|
|SATA|storahci (Inbox)|🔴 Not working|NVMe SSDs do not work with in-box storport.sys (#6, workaround available).|
|eMMC|[dwcsdhc](https://github.com/worproject/Rockchip-Windows-Drivers/tree/master/drivers/sd/dwcsdhc)|🟢 Working||
|SD/SDIO||🔴 Not working||
|CPU frequency scaling||🔴 Not working|Clocks limited at values set by UEFI.|
|HDMI output|MSBDD (Inbox)|🟡 Partially working|Single display with mode limited at 1080p 60 Hz, provided by UEFI GOP.|
|HDMI input||🔴 Not working||
|DisplayPort output|MSBDD (Inbox)|🟡 Partially working|Single display with mode limited at 1080p 60 Hz, provided by UEFI GOP. Only works in one orientation of the Type-C connector.|
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
|GPIO|[rk3xgpio](https://github.com/worproject/Rockchip-Windows-Drivers/tree/master/drivers/gpio)|🟢 Working||
|I2C|[rk3xi2c](https://github.com/worproject/Rockchip-Windows-Drivers/tree/master/drivers/i2c)|🟢 Working||
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
