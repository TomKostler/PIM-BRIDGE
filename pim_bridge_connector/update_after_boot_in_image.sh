# Mounts the disk-image that is used for the gem5 Simulation and
# updates the after_boot.sh and the userspace_programm


# ------ Mounting ------
sudo losetup -P /dev/loop0 arm-ubuntu-24.04-img
sudo kpartx -a /dev/loop0
mkdir -p /mnt/ubuntu_root
mount /dev/mapper/loop0p2 /mnt/ubuntu_root


# Update the after_boot.sh and the userspace program in the image
cp after_boot.sh /mnt/ubuntu_root/home/gem5/
cp userspace_programm_test.c /mnt/ubuntu_root/home/gem5/
chmod 777 /mnt/ubuntu_root/home/gem5/after_boot.sh


sync
sync

# ------ Unmounting and clean-up ------
cd /
umount /mnt/ubuntu_root/
losetup -d /dev/loop0
kpartx -d /dev/loop0