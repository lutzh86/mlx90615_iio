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

MODULE_INFO(intree, "Y");

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used __section(__versions) = {
	{ 0x9b1b4c2e, "module_layout" },
	{ 0x201ecc68, "iio_read_const_attr" },
	{ 0xdf154d7d, "i2c_del_driver" },
	{ 0x79c33ccc, "i2c_register_driver" },
	{ 0xdecd0b29, "__stack_chk_fail" },
	{ 0xe887de49, "i2c_smbus_xfer" },
	{ 0x8f678b07, "__stack_chk_guard" },
	{ 0x5fc262cb, "mutex_unlock" },
	{ 0x195a71c2, "mutex_lock" },
	{ 0x8f05733b, "wake_up_process" },
	{ 0x1ee8d6d4, "refcount_inc_checked" },
	{ 0xe350121a, "kthread_create_on_node" },
	{ 0x389f25fc, "iio_get_time_ns" },
	{ 0xb3f7646e, "kthread_should_stop" },
	{ 0xf9a482f9, "msleep" },
	{ 0x1e3509dd, "iio_push_to_buffers" },
	{ 0xab5a9451, "i2c_smbus_read_word_data" },
	{ 0x976ffd4b, "__put_task_struct" },
	{ 0xc1e58a5f, "refcount_dec_and_test_checked" },
	{ 0x9a4db3ef, "kthread_stop" },
	{ 0xac27b6f2, "_dev_info" },
	{ 0x6033383d, "__iio_device_register" },
	{ 0x13834de3, "iio_device_attach_buffer" },
	{ 0x6e3980ef, "devm_iio_kfifo_allocate" },
	{ 0x34a48cb2, "devm_iio_device_alloc" },
	{ 0x6772e862, "iio_device_unregister" },
	{ 0xefd6cf06, "__aeabi_unwind_cpp_pr0" },
};

MODULE_INFO(depends, "industrialio,kfifo_buf");

MODULE_ALIAS("i2c:mlx90615");

MODULE_INFO(srcversion, "52932B8A287EBAF0F2D823C");
