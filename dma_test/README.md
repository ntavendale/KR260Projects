#dma_test

Test application to read contents of DMA registers by mapping into /dev/mem.

To build:
```
fpc -odmareg dmareg.dpr
```

To run:
```
sudo dmareg
```

Must run as root. FPGA application must be loaded on Kria or it will crash system and you will need to restart.
