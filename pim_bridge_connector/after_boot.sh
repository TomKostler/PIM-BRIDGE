# Script that gets automatically loaded by the .bashrc from the 
# user 'gem5' in simulation. Inserts the PIM-BRIDGE kernel module 
# into the kernel.

#!/bin/bash

printf "In after_boot.sh...\n"

gem5-bridge --addr=0x10010000 exit

printf "Version 6.43 \n"
printf " -------------------- \n"
ls
cat after_boot.sh
printf " --------------------- \n"


printf "AFTER_BOOT.SH: Reading PIM-Bridge kernel module ... \n"
gem5-bridge --addr=0x10010000 readfile > /tmp/script
chmod 755 /tmp/script


printf "AFTER_BOOT.SH: insmod kernel module into kernel ... \n"
sudo insmod /tmp/script
printf "AFTER_BOOT.SH: insmod done\n"


printf "Creating device file ...\n"
sudo mknod /dev/pim_device c 100 0
sudo chmod 666 /dev/pim_device

printf "Syncing filesystem...\n"
sync
sleep 1

printf "Compiling User Lib ...\n"
gcc userspace_programm_test.c -o userspace_programm_test
gem5-bridge --addr=0x10010000 exit # Change cpu to O3 model
./userspace_programm_test
printf " ---------------------------------- \n"


sudo dmesg

sudo rm /dev/pim_device
rm -f /tmp/script

/bin/sh
gem5-bridge --addr=0x10010000 exit
