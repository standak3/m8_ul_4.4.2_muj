#!/bin/bash

make clean mrproper
rm -rf ../../#output/kernel_installer.zip
rm -rf ../../#output/system/
rm -rf ../../#output/kernel/
rm -rf ../../#output/boot.img

export ARCH=arm
export CROSS_COMPILE=~/kernel/kernelstock/gcc-arm-eabi-4.7/bin/arm-eabi-

make m8_defconfig
make -j8 zImage dtbs modules

#dtb
../dtbtool -p scripts/dtc/ -s 1024 -d "htc,project-id = <" -o boot.img-dtb arch/arm/boot/

FILE=arch/arm/boot/zImage

if [ -f $FILE ];
then
mkdir ../../#output/kernel
mkdir ../../#output/system
mkdir ../../#output/system/lib
mkdir ../../#output/system/lib/modules
cp -rf arch/arm/boot/zImage ~/kernel/kitchen/split_img/boot.img-zImage
cp -rf boot.img-dtb ~/kernel/kitchen/split_img/boot.img-dtb
find . -name '*.ko' -exec cp {} ../../#output/system/lib/modules/  \;
cp .config ../../#config/$(date +"%d_%b_%y_%H_%M_%S").config
cp ~/kernel/kernel_installer.zip ~/kernel/#output/kernel_installer.zip
cd ~/kernel/kitchen
r.sh
fi
