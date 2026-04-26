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

## Git Access
Start ssh agent:
```
 eval "$(ssh-agent -s)"
```

Add private key:
```
ssh-add ~/.ssh/<my_key_Name>
```

Clone using ssh:

```
 git clone ssh://git@ssh.github.com:443/ntavendale/KR260Projects.git
 ```



