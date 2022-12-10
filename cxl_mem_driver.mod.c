#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;
BUILD_LTO_INFO;

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
	{ 0xf704969, "module_layout" },
	{ 0x37a0cba, "kfree" },
	{ 0xf97f0ac2, "pci_unregister_driver" },
	{ 0xc946dda0, "cdev_del" },
	{ 0x933c4a18, "class_destroy" },
	{ 0x82e7bb9c, "device_destroy" },
	{ 0x193e3022, "__pci_register_driver" },
	{ 0x54b1fac6, "__ubsan_handle_load_invalid_value" },
	{ 0xec845c70, "remap_pfn_range" },
	{ 0x18554f24, "current_task" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0x87a21cb3, "__ubsan_handle_out_of_bounds" },
	{ 0xdaa2a098, "pci_request_regions" },
	{ 0x698c665b, "pci_enable_device" },
	{ 0xefc94da8, "device_create" },
	{ 0x325cb5cb, "__class_create" },
	{ 0xc378cea7, "cdev_add" },
	{ 0x2d725fd4, "cdev_init" },
	{ 0x3fd78f3b, "register_chrdev_region" },
	{ 0x7c797b6, "kmem_cache_alloc_trace" },
	{ 0xd731cdd9, "kmalloc_caches" },
	{ 0x7b9a2346, "pci_disable_device" },
	{ 0xfb2cfc1, "pci_release_regions" },
	{ 0x92997ed8, "_printk" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0xbdfb6dbb, "__fentry__" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "B1D8ACD01E64BF33BA8F5A5");
