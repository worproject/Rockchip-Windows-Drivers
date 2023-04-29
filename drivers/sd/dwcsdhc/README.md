# Synopsys DesignWare SD/eMMC Host Controller Driver
This is a driver for the DesignWare Core SD/eMMC Host Controller found in RK35xx SoCs.

It is a miniport that works in conjunction with sdport.sys, which implements the actual SD/SDIO/eMMC protocols and WDM interfaces.

## Hardware details
The controller itself is SDHCI-compliant, with some peculiarities and a few more on Rockchip's implementation:
- HS400 has a custom setting.
- the clock divider is not functional, requiring clock rate changes via CRU instead.
- the PHY Delay Lines (DLL) need to be configured depending on bus speed mode, especially for HS200/HS400(ES).

## Supported features
- ADMA2 transfers
- up to HS SDR52 speed mode (52 MB/s)

## To-do
* HS400 Enhanced Strobe (the other modes are not planned)

## Credits
The driver is based on Microsoft's [sdhc sample](https://github.com/microsoft/Windows-driver-samples/tree/main/sd/miniport/sdhc) with a few fixes from [bcm2836sdhc](https://github.com/raspberrypi/windows-drivers/tree/master/drivers/sd/bcm2836/bcm2836sdhc) in regard to request completion and interrupts handling.
