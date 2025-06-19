Introduction
============

   rf-util repository contains the host software components for
   supporting Metanoia MT3812 RFIC. It includes a python based user space CLI,
   which can be used to configure the MT3812 using SPI interface on RFNM hardware.
   This repository includes MT3812 drivers as git submodule. 

   This repository uses the Open Source BSD-3-Clause license for the user space
   libraries and applications. 

Directory structure
===================

rf-util/

   ─ python          : contains python CLI and default RF configuration 
   
   ─ pyincludes      : contains python config file aligned with NXP LA9310 BSP Drop 1.0 (python3.10)
   
   ─ interface       : contains RFIC generic api using rflib
   
   ─ ext             : contains git submodule of MT3812 driver
   
   ─ port            : contains MT3812 driver adaptation layer for RFNM hardware
   
   ─ rflib           : contains MT3810 control code  
   
   ─ license         : contains license
   
   ─ scripts         : contains init script to be used on target

Build Instructions
==================

1.  create work space folder and export
    export WORKDIR=$PWD

2.  Need to generate spi user space libraty part of la93xx_host_sw repo
    git clone  https://github.com/nxp-qoriq/la93xx_host_sw.git
    cd la93xx_host_sw
    git checkout -b imx-la93xx-1.0 imx-la93xx-1.0
    cd lib/ecspi
    API_DIR=../../api CC=aarch64-linux-gnu-gcc CFLAGS=-Wno-unused-variable LIB_INSTALL_DIR=. make
    cd $WORKDIR

3.  Clone rf-util and submodules  
    git clone https://github.com/nxp-qoriq/rf-util.git
    cd rf-util
    git checkout -b os_mt38xx_dev origin/os_mt38xx_dev
    git submodule update --init --recursive

4.  Generate MT3812 driver library
    cd ext/diora-sdk-open-source
    patch -p 1 < ../diora-sdk-remove-al-stub.patch
    CC=aarch64-linux-gnu-gcc make

5.  Need Python includes aligned with NXP LA9310 BSP Drop 1.0 (python3.10.7)
    wget https://www.python.org/ftp/python/3.10.7/Python-3.10.7.tgz
    tar -xvf Python-3.10.7.tgz
    cd $WORKDIR

6.  Need la93xx_freertos for common_headers/la9310_modinfo.h
    git clone https://github.com/nxp-qoriq/la93xx_freertos.git
    cd la93xx_freertos/
    git checkout -b imx-la93xx-1.0 imx-la93xx-1.0
    cd $WORKDIR
	 
7.  Genrate rf-util
    cd rf-util
    export ECSPI_LIB_PATH=$WORKDIR/la93xx_host_sw/lib/ecspi
    export LA9310_COMMON_HEADERS=$WORKDIR/la93xx_freertos/common_headers
    export KERNEL_DIR=$WORKDIR/linux
    export CROSS_COMPILE=aarch64-linux-gnu-
    export ARCH=arm64
    export PYTHON_INCLUDE=$WORKDIR/Python-3.10.7/Include
    make MT3812=1 clean
    make MT3812=1 
    make MT3812=1 install

8.  Deploy rf-util on the target
    rm -r rf_ctrl/
    cp -rf install/home/root/rf-ctrl/ ./rf_ctrl/
    tar -czvf rflib.tar.gz ./rf_ctrl/*
    scp rflib.tar.gz root@<BoardIP>:/home/root


How to Deploy and use rf-util 
==============================

a.   Deploy prebuilt image for i.MX8MP-LA9310 BSP v1.0 
     prebuilt image includes prebuilt image of geul_rf_util and MT3812 drivers
	 
     wget https://www.nxp.com/lgfiles/sdk/la1224/imx-la9310-sdk-10/nxp-image-real-time-edge-imx8mp-sdr.rootfs.wic.zst
     zstd -d nxp-image-real-time-edge-imx8mp-sdr.rootfs.wic.zst 
     sudo dd if= nxp-image-real-time-edge-imx8mp-sdr.rootfs.wic of=/dev/sdb bs=8M oflag=direct status=progress
     Sync
     insert sdcard and change dipswitch on “sd”, and it should have NXP image running

b.   Tx config example (using fr1fr2 tool)

   .1 Initialize LA9310   
     cd /home/root/rf_ctrl
     ./utils/scripts/enable_rf.sh
	 
   .2 use python CLI to configure RFIC  
     export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/root/rf_ctrl/fr1/
     python3 ./fr1/fr1.py 
       set_log_level 3
       rf_init ./utils/fr1/fr1_def_config.json
       set_active act_mode 0 receiver 0
       set_path path 0 band 2 rssi 0 dpd 0 rx_bw 4 tx_bw 0
       set_path path 12 band 2 rssi 0 dpd 0 rx_bw 4 tx_bw 0
       set_active act_mode 1 receiver 0
       q
	   
   .3 use linux sysfs entry to configure RF path between MT3812 and Antenna  
     echo sma_a > /sys/kernel/rfnm_primary/tx0/path
     echo on > /sys/kernel/rfnm_primary/tx0/enable
     echo 1 > /sys/kernel/rfnm_primary/tx0/apply

   .4 use fr1fr2 tool to play 5G waveform
     cd /home/root/fr1_fr2_test_tool/
     ./channels_start.sh

c.   Rx config example (using iq-player)

   .1 Initialize LA9310 
     cd /home/root/host_utils
     ./load-nlm.sh
     
   .2 use python CLI to configure RFIC
     cd /home/root/rf_ctrl
     export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/root/rf_ctrl/fr1/
     python3 ./fr1/fr1.py 
       set_log_level 3
       rf_init ./utils/fr1/fr1_def_config.json
       set_active act_mode 0 receiver 0
       set_path path 0 band 2 rssi 0 dpd 0 rx_bw 4 tx_bw 0
       set_path path 15 band 2 rssi 0 dpd 0 rx_bw 4 tx_bw 0
       set_active act_mode 1 receiver 0

   .3 use linux sysfs entry to configure RF path between MT3812 and Antenna
     echo sma_a > /sys/kernel/rfnm_primary/rx0/path
     echo on > /sys/kernel/rfnm_primary/rx0/enable
     echo 1 > /sys/kernel/rfnm_primary/rx0/apply

   .4 use iq-player to capture IQ samples to a file     
     cd /home/root/host_utils
     ./iq-capture.sh ./iqdata.bin 1200


