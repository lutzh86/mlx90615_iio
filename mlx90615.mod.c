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
	{ 0x8c43053d, "module_layout" },
	{ 0x2c3ce373, "iio_read_const_attr" },
	{ 0xe65516f, "i2c_del_driver" },
	{ 0x2221fa59, "i2c_register_driver" },
	{ 0x5fc262cb, "mutex_unlock" },
	{ 0x195a71c2, "mutex_lock" },
	{ 0x683e8659, "i2c_smbus_read_word_data" },
	{ 0xf9a482f9, "msleep" },
	{ 0x8f913b22, "i2c_smbus_xfer" },
	{ 0x5ce0a7f2, "wake_up_process" },
	{ 0x1ee8d6d4, "refcount_inc_checked" },
	{ 0xe350121a, "kthread_create_on_node" },
	{ 0xdecd0b29, "__stack_chk_fail" },
	{ 0xc5850110, "printk" },
	{ 0xf0e6653e, "iio_push_to_buffers" },
	{ 0x9054d1df, "i2c_smbus_read_i2c_block_data" },
	{ 0xd0ff3795, "iio_get_time_ns" },
	{ 0xb3f7646e, "kthread_should_stop" },
	{ 0xcc5005fe, "msleep_interruptible" },
	{ 0x8f678b07, "__stack_chk_guard" },
	{ 0x2c4810c4, "__put_task_struct" },
	{ 0xc1e58a5f, "refcount_dec_and_test_checked" },
	{ 0x9a4db3ef, "kthread_stop" },
	{ 0xe0dd80b3, "_dev_info" },
	{ 0xb014d02b, "__iio_device_register" },
	{ 0x22cc7a9f, "iio_device_attach_buffer" },
	{ 0x8f8f8073, "devm_iio_kfifo_allocate" },
	{ 0x604a81fb, "devm_iio_device_alloc" },
	{ 0x2fc19740, "iio_device_unregister" },
	{ 0xefd6cf06, "__aeabi_unwind_cpp_pr0" },
};

MODULE_INFO(depends, "industrialio,kfifo_buf");

MODULE_ALIAS("i2c:mlx90615");

MODULE_INFO(srcversion, "BE36D95C71CD00441F94232");
