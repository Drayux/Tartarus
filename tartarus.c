#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

static int __init driver_init(void) {
	printk(KERN_INFO, "Hello world\n");
	return 0;
}

static void __exit driver_exit(void) {
	
}

module_init(driver_init);
module_exit(driver_exit);

MODULE_AUTHOR("Drayux");
MODULE_DESCRIPTION("Tartarus V2 Driver");
MODULE_LICENSE("GPL");