#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tom Kostler");
MODULE_DESCRIPTION("Kernel-Module acting as a driver for PIM-Functionality");


extern int test_vadd(void);

static int __init hello_init(void) {
    printk(KERN_INFO "Hello World!\n");

    int result = test_vadd();
    if (result == 42)
        printk(KERN_INFO "VADD erfolgreich!!!");

    return 0;
}

static void __exit hello_exit(void) {
    printk(KERN_INFO "Bye, World!\n");
}

module_init(hello_init);
module_exit(hello_exit);
