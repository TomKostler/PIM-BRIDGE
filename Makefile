# Use same linux version as target image for gem5 full system sinmulation
KDIR ?= ../linux-5.15.36
PARAMS ?= 


default:
	$(MAKE) -C $(KDIR) M=$$PWD

modules_install: default
	$(MAKE) -C $(KDIR) M=$$PWD modules_install

clean:
	sudo make -C $(KDIR) M=$(shell pwd) clean

install:
	# sudo insmod pim_bridge_module.ko $(PARAMS)
	sudo cp pim_bridge_module.ko ../gem5-pim/pim_bridge_connector/pim_bridge_module.ko

# remove:
# 	sudo rmmod pim_bridge_module.ko
