#!/bin/bash

# 安装模块
echo "=============== Install modules ==============="
sudo env PATH=$PATH make -j64 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- INSTALL_MOD_PATH=mnt/root modules_install


echo "=============== 备份旧内核 ==============="
sudo cp mnt/boot/$KERNEL.img mnt/boot/$KERNEL-backup.img # 备份旧内核
echo "=============== 拷贝新内核 ==============="
sudo cp arch/arm64/boot/Image mnt/boot/$KERNEL.img # 拷贝新内核
echo "=============== 拷贝设备树文件 ==============="
sudo cp arch/arm64/boot/dts/broadcom/*.dtb mnt/boot/ # 拷贝设备树文件
echo "=============== 拷贝设备树覆盖文件 ==============="
sudo cp arch/arm64/boot/dts/overlays/*.dtb* mnt/boot/overlays/ # 拷贝设备树覆盖文件
echo "=============== 拷贝设备树覆盖文件说明 ==============="
sudo cp arch/arm64/boot/dts/overlays/README mnt/boot/overlays/ # 拷贝设备树覆盖文件说明

echo "=============== Done! ==============="
