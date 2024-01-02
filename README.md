# Windows on Arm device drivers for Rockchip
This repository contains drivers for RK35xx-based platforms, with a focus on RK3588(S).

## Hardware support status
|Device|Driver|Status|Additional information|
| --- | --- | --- | --- |
|USB 3 ports 0/1 Host|usbxhci (Inbox)|🟢 Working|The "full" USB3 port(s) work correctly, except USB3 only works in one orientation when used with a Type-C connector.<br> Note that RK3588s devices (e.g. Opi5, Opi5B) have only 1 "full" USB3 port -- only RK3588 devices (e.g. Opi5+) have 2 "full" USB3 ports.|
|USB 3 ports 0/1 Dual Role|usbxhci (Inbox)|🔴 Not working|Host mode only, no dual role capability. Depends on USB/DP Alt Mode switching.|
|USB 3 port 2 Host|usbxhci (Inbox)|🟡 Partially working|USB3 only, won't support USB2 or USB1 devices even if you use a hub.<br> USB3 port 2 works by combining a USB3-only xHCI port with a USB2 EHCI+OHCI port. Since EHCI+OHCI aren't working (issue #4), this port will work for USB3 devices but not for USB2 or USB1 devices.|
|USB 2.0 & 1.1|usbehci (Inbox)|🔴 Not working|Disabled by default in UEFI.<br> Windows bugchecks if EHCI device enabled (issue #4).<br> USBOHCI driver for USB 1.1 is missing in ARM64 builds (issue #5).|
|PCIe 3.0 & 2.1|pci (Inbox)|🟡 Partially working|Devices may work if drivers are available for them. Known issues include:<br> - NVMe SSDs do not work with in-box storport.sys (issue #6, workaround available).<br> - Devices that require cache-coherent bus or MSI do not work (e.g. Qualcomm Wi-Fi cards).<br> - Devices that require a root PCIe port do not work (e.g. XHCI).|
|SATA|storahci (Inbox)|🔴 Not working|SATA SSDs do not work with in-box storport.sys (issue #6, workaround available).|
|eMMC|[dwcsdhc](https://github.com/worproject/Rockchip-Windows-Drivers/tree/master/drivers/sd/dwcsdhc)|🟢 Working||
|SD/SDIO||🔴 Not working||
|CPU frequency scaling||🔴 Not working|Clocks limited at values set by UEFI.|
|HDMI output|MSBDD (Inbox)|🟡 Partially working|Single display with mode limited at 1080p 60 Hz, provided by UEFI GOP.|
|HDMI input||🔴 Not working||
|DisplayPort output|MSBDD (Inbox)|🟡 Partially working|Single display with mode limited at 1080p 60 Hz, provided by UEFI GOP. Only works in one orientation of the Type-C connector.|
|HDMI audio||🔴 WIP|I2S audio driver enumerates, but requires VOP driver|
|DisplayPort audio||🔴 Not working||
|Analog audio|[es8323](https://github.com/worproject/Rockchip-Windows-Drivers/tree/master/drivers/audio/codecs/es8323)|🟢 Working (Orange Pi 5)||
|Digital audio||🔴 Not working||
|USB/DP Alt Mode||🔴 Not working||
|GPU||🔴 Not working|Software-rendered|
|NPU||🔴 Not working||
|Multimedia codecs||🔴 Not working||
|DSI||🔴 Not working||
|CSI||🔴 Not working||
|GMAC Ethernet|[dwc_eqos](drivers/net/dwc_eqos)|🟢 Working|Requires latest UEFI (edk2-rk3588 master 2024/01/03 or later).<br> Note that this driver supports the on-chip Ethernet (e.g. it supports the 1GB Ethernet port on Opi5) but does not support the separate Ethernet chip that is added to some higher-tier boards (e.g. it does not support the 2.5GB Ethernet ports on Opi5+).
|UART||🔴 Not working|No OS driver but debugging does work on UART2, being configured by UEFI.|
|GPIO|[rk3xgpio](https://github.com/worproject/Rockchip-Windows-Drivers/tree/master/drivers/gpio)|🟢 Working||
|I2C|[rk3xi2c](https://github.com/worproject/Rockchip-Windows-Drivers/tree/master/drivers/i2c)|🟢 Working||
|I2S|[csaudiork3x](https://github.com/worproject/Rockchip-Windows-Drivers/tree/master/drivers/audio/csaudiork3x)|🟢 Working||
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
