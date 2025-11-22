# rtl8822cs-20240221
(**WIP**) RTL8822CS Linux Driver v5.15.8.3-17-20240221 FPV Mod 

Tested:
 - Build on kernel 6.8
 - TX power
 - TX injection in monitor mode
 - Narrowband support in monitor mode, 5MHz/10MHz bandwidth
 - Custom hardware (by using SCH-SM822C00-5V3.pdf below)

Need test:
 - EDCCA patch
 - RX in monitor mode (reason: [this patch](https://github.com/libc0607/rtl8822cs-20240221/commit/aef67b1363f7988e2bdee2a567c412947ee2dea2))
 - Thermal sensor default offset
 - Monitor mode transmit beamforming bf_mon.sh (compatible with my [88x2cu](https://github.com/libc0607/rtl88x2cu-20230728)/[88x2eu](https://github.com/libc0607/rtl88x2eu-20230815))
 - Channel state scanning
 - ...

Known bugs/issues:
 - Injection instability on 40MHz channels -- same as 88x2cu/88x2eu. help wanted
 - ```iw set 5/10MHz``` may report invalid on some old kernels (e.g. Ingenic T31 3.10.14) -- that's not a driver issue. At that time 5/10MHz BW was invalid in the kernel (see [here](https://elixir.bootlin.com/linux/v3.10.14/source/net/wireless/chan.c#L47)). Use ```/proc/.../monitor_chan_override```.

## Platform Compatibilities 
Only tested platforms are listed. 

### OpenIPC, Ingenic T31, MSC1
1. Add this package to OpenIPC firmware: [rtl8822cs-openipc-fpv](https://github.com/libc0607/openipc-firmware/tree/master/general/package/rtl8822cs-openipc-fpv)  
2. Set `BR2_PACKAGE_RTL8822CS_OPENIPC_FPV=y` in config  
3. Rebuild, flash the firmware  
4. After boot, run `modprobe 88x2cs` first, then the adapter should show in `ifconfig -a`.  

(The Ingenic SDIO controller driver needs a manual re-probe (`jzmmc_manual_detect()`) when `insmod`, which should be explicitly called in `platform_wifi_power_on` [here](https://github.com/libc0607/rtl8822cs-20240221/commit/b0516d18d5d405362035ec9c18393c276d953d1f#diff-76ed074a9305c04054cdebb9e9aad2d818052b07091de1f20cad0bbac34ffb52R197). So we need to modprobe first)  

### OpenIPC, Sigmastar SSC338Q, SDIO1 
1. Add this package to OpenIPC firmware: [rtl8822cs-openipc-fpv](https://github.com/libc0607/openipc-firmware/tree/master/general/package/rtl8822cs-openipc-fpv)  
2. Set `BR2_PACKAGE_RTL8822CS_OPENIPC_FPV=y` in config  
3. Modify [infinity6e-ssc012b-s01a.dts](https://github.com/OpenIPC/linux/blob/b6d62526ce52d5ae594c7b9b6683cbe3fb3f5e8f/arch/arm/boot/dts/infinity6e-ssc012b-s01a.dts#L131): in `sdmmc` section, set `slot-fakecdzs = <0>,<1>,<0>;` and `slot-sdio-use = <0>,<1>,<0>;`  
4. Make sure the pinmux for all SDIO1 pins is set to `PINMUX_FOR_SD1_...` [here](https://github.com/OpenIPC/linux/blob/b6d62526ce52d5ae594c7b9b6683cbe3fb3f5e8f/arch/arm/boot/dts/infinity6e-padmux-qfn.dtsi#L64)   
5. Rebuild, flash the firmware  
6. Check hardware: Puddings' SDIO1 needs external pull-up resistors -- make sure that they've all pulled up with ~10kOhms; And make sure VDDIO is 3.3v on both sides  
7. After boot, run `find /sys/bus/sdio/devices/*|xargs -I {} cat {}/uevent|grep "SDIO_ID"|cut -d= -f2`; If nothing wrong, the output should be `024C:C822`  
8. Run `modprobe 88x2cs`, then the adapter should show in `ifconfig -a`.  

## Documents 
Note: These documents are all collected from the Internet. Use them at your own risk. Contact me to delete them if ...  
 - [BL-M8822CS5-BT5.0_Product Specification_V1.0.pdf](https://github.com/user-attachments/files/18177652/BL-M8822CS5-BT5.0_Product.Specification_V1.0.pdf)  
 - [RTL8822CS-VS-CG-DataSheet_v0.1r07_20190726_248218.pdf](https://github.com/user-attachments/files/18177662/00016708-RTL8822CS-VS-CG-DataSheet_v0.1r07_20190726_248218.pdf)  
 - [RTL8822CS-VL-CG-DataSheet_v0.3r02_20190423_248219.pdf](https://github.com/user-attachments/files/18177661/00016898-RTL8822CS-VL-CG-DataSheet_v0.3r02_20190423_248219.pdf)  
 - [SCH-SM822C00-5V3.pdf](https://github.com/user-attachments/files/18177675/SCH-SM822C00-5V3.pdf)  
 - [SCH-SM822C01-2V1.pdf](https://github.com/user-attachments/files/18177677/SCH-SM822C01-2V1.pdf)  
