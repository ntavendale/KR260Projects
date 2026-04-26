# KR260Projects

## List Firmware Apps
```
sudo xnmutil listapps 
```
Eeach app has it's own folder in the /lib/firmware/xilinx directory.

## Set Default Firmware

Update the following file:

```
/etc/dfx-mgrd/default_firmware
```

## Build dmareg
cd to KR260Projects/test_dma directory

```
fpc -odmareg dmareg.dpr
```

