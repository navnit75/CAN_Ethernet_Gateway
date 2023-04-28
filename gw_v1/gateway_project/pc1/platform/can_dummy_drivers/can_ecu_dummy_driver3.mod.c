#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0xbe50313b, "module_layout" },
	{ 0x7ffef792, "sh_buf3" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0xb48515a2, "class_destroy" },
	{ 0x5df2a63d, "class_unregister" },
	{ 0x1bd78031, "device_destroy" },
	{ 0xc959d152, "__stack_chk_fail" },
	{ 0x5a7eba18, "device_create" },
	{ 0xcfc316d1, "cdev_add" },
	{ 0x4b743d2a, "cdev_init" },
	{ 0xc0880c2b, "__class_create" },
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0x362ef408, "_copy_from_user" },
	{ 0xae0d55d1, "can3_wrlock" },
	{ 0x9c6febfc, "add_uevent_var" },
	{ 0x409bcb62, "mutex_unlock" },
	{ 0xb44ad4b3, "_copy_to_user" },
	{ 0x88db9f48, "__check_object_size" },
	{ 0x2ab7989d, "mutex_lock" },
	{ 0x29ba9bf0, "can3_rdlock" },
	{ 0xc5850110, "printk" },
	{ 0xbdfb6dbb, "__fentry__" },
};

MODULE_INFO(depends, "gw_ecu_dummy_driver3");


MODULE_INFO(srcversion, "C4D661E141F6C337B1E39EB");
