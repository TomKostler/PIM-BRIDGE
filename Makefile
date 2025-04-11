# Makefile for including another C-Source File into the Module

KDIR ?= ../linux # Default if KDIR is not set from outside
RUST_ENV := sudo RUSTUP_TOOLCHAIN=stable PATH="$(HOME)/.cargo/bin:$$PATH"

default:
	$(RUST_ENV) $(MAKE) -C $(KDIR) M=$$PWD

modules_install: default
	$(RUST_ENV) $(MAKE) -C $(KDIR) M=$$PWD modules_install

clean:
	sudo make -C $(KDIR) M=$(shell pwd) clean

install:
	sudo insmod pim_bridge_module.ko

remove:
	sudo rmmod pim_bridge_module.ko
