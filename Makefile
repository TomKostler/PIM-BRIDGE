obj-m += pim_bridge_module.o
RUST_LIB := ../PIM-OS/pim-os/kernels/X1/aarch64-unknown-none/release/libpim_os.a

all:
	sudo make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) modules EXTRA_CFLAGS="-I$(PWD)" LDFLAGS_MODULE="$(RUST_LIB)"

clean:
	sudo make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) clean


install:
	sudo insmod pim_bridge_module.ko

remove:
	sudo rmmod pim_bridge_module.ko