#include <linux/build-salt.h>
#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(.gnu.linkonce.this_module) = {
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
__used __section(__versions) = {
	{ 0xfece093d, "module_layout" },
	{ 0xfff5e3e8, "iio_read_const_attr" },
	{ 0x51c503e0, "i2c_del_driver" },
	{ 0x25c0e901, "i2c_register_driver" },
	{ 0x5fc262cb, "mutex_unlock" },
	{ 0x12b19955, "i2c_smbus_read_word_data" },
	{ 0x195a71c2, "mutex_lock" },
	{ 0x8f678b07, "__stack_chk_guard" },
	{ 0xdecd0b29, "__stack_chk_fail" },
	{ 0xf9a482f9, "msleep" },
	{ 0x4829e776, "i2c_smbus_xfer" },
	{ 0xd4430003, "__iio_device_register" },
	{ 0x4a175f8, "devm_iio_device_alloc" },
	{ 0x3d0b19b, "iio_device_unregister" },
	{ 0xb1ad28e0, "__gnu_mcount_nc" },
};

MODULE_INFO(depends, "industrialio");

MODULE_ALIAS("i2c:mlx90615");

MODULE_INFO(srcversion, "2E699A04942414007816794");
