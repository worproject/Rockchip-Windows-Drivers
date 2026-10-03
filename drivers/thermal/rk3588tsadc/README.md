RK3588 TSADC Temperature Sensor Driver

Reports the temperature of the TSADC package sensor (channel 0) to a Windows thermal zone, through IOCTL_THERMAL_READ_TEMPERATURE.

The UEFI has to start the TSADC and describe it as ACPI device RKCP3004 (registers in _CRS), and the thermal zone has to name that device in the Microsoft thermal extensions _DSM (function 2) and in _DEP. Windows does not poll _TMP, so without this driver the thermal zone keeps the temperature it read at boot.

Tested on ROCK 5T.
