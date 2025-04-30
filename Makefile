KDIR ?= ../linux
PARAMS ?= 

default:
	$(MAKE) -C $(KDIR) M=$$PWD

modules_install: default
	$(MAKE) -C $(KDIR) M=$$PWD modules_install

clean:
	sudo make -C $(KDIR) M=$(shell pwd) clean

install:
	sudo insmod pim_bridge_module.ko $(PARAMS)

remove:
	sudo rmmod pim_bridge_module.ko

