/*
 * mlx90615.c - Support for Melexis MLX90615 contactless IR temperature sensor
 *
 *
 *
 *
 * Heavily based on mlx90614 driver from:
 *        Copyright (c) 2014 Peter Meerwald <pmeerw@pmeerw.net>,
 *        Copyright (c) 2015 Essensium NV
 *        Copyright (c) 2015 Melexis
 *
 * Copyright (c) 2020 SHPI GmbH
 *
 *
 * This file is subject to the terms and conditions of version 2 of
 * the GNU General Public License.  See the file COPYING in the main
 * directory of this archive for more details.
 *
 * Driver for the Melexis MLX90615 I2C 16-bit IR thermopile sensor
 *
 * (7-bit I2C slave address 0x5b, 10 - 100KHz bus speed only!)
 *
 *
 */

#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/jiffies.h>

#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/buffer.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/kthread.h>
#include <linux/sched/task.h>

#define MLX90615_OP_RAM           0x20
#define MLX90615_OP_EEPROM        0x10
#define MLX90615_OP_SLEEP         0xC6       /*SCL needs to be high during Sleep,  Enter sleep mode command (SA=0x5B, Command=0xC6,PEC=0x6D) */

#define MLX90615_ADDR           (MLX90615_OP_EEPROM | 0x00) /* slave adress 0x5B, do not change unless you know what youre doing!*/

#define MLX90615_CONFIG_IIR_SHIFT 12 /* IIR coefficient */
#define MLX90615_CONFIG_IIR_MASK (0x7 << MLX90615_CONFIG_IIR_SHIFT)

/*

IIR 3 Bits

    data = data & 0x7000;
    data = data >> 12;

000	0	forbidden
001	1	100%         0.10s     1.0
010	2	50%          0.86s     0.5
011	3	33.336%      1.4s      0.33
100	4	25%          2.00s     0.25
101	5	20%          3.00s     0.20
110	6	16.666%      3.3s      0.16
111	7	14.286%      4.5s      0.14

cuts off spikes 100% means no cut off 


*/


#define MLX90615_EMISSIVITY     (MLX90615_OP_EEPROM | 0x03) /* emissivity correction coefficient, Emissivity = dec2hex(round(16384×ε )) */
#define MLX90615_CONFIG         (MLX90615_OP_EEPROM | 0x02) /* configuration register */
#define MLX90615_RAW            (MLX90615_OP_RAM | 0x05) /* raw data IR channel , Raw IR data is in sign (1 bit, the MSB) and magnitude (15 bits) format */
#define MLX90615_TA             (MLX90615_OP_RAM | 0x06) /* ambient temperature */
#define MLX90615_TOBJ           (MLX90615_OP_RAM | 0x07) /* object temperature */

#define MLX90615_TIMING_EEPROM    10 /* time for EEPROM write/erase to complete */
#define MLX90615_TIMING_WAKEUP    39 /* time to hold SDA low for wake-up */
#define MLX90615_TIMING_STARTUP  300 /* time before first data after wake-up */

#define MLX90615_CONST_OFFSET_DEC -13657 /* decimal part of the Kelvin offset */
#define MLX90615_CONST_OFFSET_REM 500000 /* remainder of offset (273.15*50) */
#define MLX90615_CONST_SCALE 20 /* Scale in milliKelvin (0.02 * 1000) */
#define MLX90615_CONST_RAW_EMISSIVITY_MAX 16384 /* max value for emissivity */
#define MLX90615_CONST_EMISSIVITY_RESOLUTION 61035 /* 1/16384 ~ 0.000061035 */

/* Bandwidth values for IIR filtering */
static const int mlx90615_iir_values[] = {0, 100, 50, 33, 25, 20, 16,  14};       /* IIR spikes in %  */

static IIO_CONST_ATTR(in_temp_object_filter_low_pass_3db_frequency_available,
		       "1.0 0.5 0.33 0.25 0.2 0.16 0.14");

struct mlx90615_data {
	struct i2c_client *client;
	struct mutex lock; /* for EEPROM access only */
	unsigned long ready_timestamp; /* in jiffies */
        struct task_struct *task;
        struct {
		u16 chan[2];
		u64 ts __aligned(8);
	} scan;
};



static struct attribute *mlx90615_attributes[] = {
	&iio_const_attr_in_temp_object_filter_low_pass_3db_frequency_available.dev_attr.attr,
	NULL,
};

static const struct attribute_group mlx90615_attr_group = {
	.attrs = mlx90615_attributes,
};

/*
 * Erase an address and write word.
 * The mutex must be locked before calling.
 */
static s32 mlx90615_write_word(const struct i2c_client *client, u8 command,
			       u16 value)
{
	/*
	 * Note: The mlx90615 requires a PEC on writing but does not send us a
	 * valid PEC on reading.  Hence, we cannot set I2C_CLIENT_PEC in
	 * i2c_client.flags.  As a workaround, we use i2c_smbus_xfer here.
	 */
	union i2c_smbus_data data;
	s32 ret;

	dev_dbg(&client->dev, "Writing 0x%x to address 0x%x", value, command);

	data.word = 0x0000; /* erase command */
	ret = i2c_smbus_xfer(client->adapter, client->addr,
			     client->flags | I2C_CLIENT_PEC,
			     I2C_SMBUS_WRITE, command,
			     I2C_SMBUS_WORD_DATA, &data);
	if (ret < 0)
		return ret;

	msleep(MLX90615_TIMING_EEPROM);

	data.word = value; /* actual write */
	ret = i2c_smbus_xfer(client->adapter, client->addr,
			     client->flags | I2C_CLIENT_PEC,
			     I2C_SMBUS_WRITE, command,
			     I2C_SMBUS_WORD_DATA, &data);

	msleep(MLX90615_TIMING_EEPROM);

	return ret;
}

/*
 * Find the IIR value inside mlx90615_iir_values array and return its position
 * which is equivalent to the bit value in sensor register
 */
static inline s32 mlx90615_iir_search(const struct i2c_client *client,
				      int value)
{
	int i;
	s32 ret;

	for (i = 1; i < ARRAY_SIZE(mlx90615_iir_values); ++i) {
		if (value == mlx90615_iir_values[i])
			break;
	}

	if (i == ARRAY_SIZE(mlx90615_iir_values))
		return -EINVAL;

	/*
	 * CONFIG register values must not be changed so
	 * we must read them before we actually write
	 * changes
	 */
	ret = i2c_smbus_read_word_data(client, MLX90615_CONFIG);
	if (ret < 0)
		return ret;

	/* Write changed values */
  /* ret = ret & 0x8FFF;
   ret = ret + (i << MLX90615_CONFIG_IIR_SHIFT);     */
   

  	ret &= ~MLX90615_CONFIG_IIR_MASK;
  	ret |= i << MLX90615_CONFIG_IIR_SHIFT;

	/* Write changed values */

   ret = mlx90615_write_word(client, MLX90615_CONFIG,ret);       
    
      
	return ret;
}


static int mlx90615_read_raw(struct iio_dev *indio_dev,
			    struct iio_chan_spec const *channel, int *val,
			    int *val2, long mask)
{
	struct mlx90615_data *data = iio_priv(indio_dev);
	u8 cmd;
	s32 ret;

	switch (mask) {
	case IIO_CHAN_INFO_RAW: /* 0.02K / LSB */
		switch (channel->channel2) {
		case IIO_MOD_TEMP_AMBIENT:
			cmd = MLX90615_TA;
			break;
		case IIO_MOD_TEMP_OBJECT:
                        cmd = MLX90615_TOBJ;
			break;
		default:
			return -EINVAL;
		}


		ret = i2c_smbus_read_word_data(data->client, cmd);

		if (ret < 0)
			return ret;

		/* MSB is an error flag */
		if (ret & 0x8000)
    		/* if (ret > 0x7FFF)*/
			return -EIO;

		*val = ret;
		return IIO_VAL_INT;
	case IIO_CHAN_INFO_OFFSET:
		*val = MLX90615_CONST_OFFSET_DEC;
		*val2 = MLX90615_CONST_OFFSET_REM;
		return IIO_VAL_INT_PLUS_MICRO;
	case IIO_CHAN_INFO_SCALE:
		*val = MLX90615_CONST_SCALE;
		return IIO_VAL_INT;
	case IIO_CHAN_INFO_CALIBEMISSIVITY: /* 1/16384 / LSB */

		mutex_lock(&data->lock);
		ret = i2c_smbus_read_word_data(data->client,
					       MLX90615_EMISSIVITY);
		mutex_unlock(&data->lock);

		if (ret < 0)
			return ret;

		if (ret == MLX90615_CONST_RAW_EMISSIVITY_MAX) {
			*val = 1;
			*val2 = 0;
		} else {
			*val = 0;
			*val2 = ret * MLX90615_CONST_EMISSIVITY_RESOLUTION;
		}
		return IIO_VAL_INT_PLUS_NANO;
	case IIO_CHAN_INFO_LOW_PASS_FILTER_3DB_FREQUENCY: 
		mutex_lock(&data->lock);
		ret = i2c_smbus_read_word_data(data->client, MLX90615_CONFIG);
		mutex_unlock(&data->lock);

		if (ret < 0)
			return ret;

		*val = mlx90615_iir_values[(ret & MLX90615_CONFIG_IIR_MASK) >> MLX90615_CONFIG_IIR_SHIFT ] / 100;
		*val2 = (mlx90615_iir_values[(ret & MLX90615_CONFIG_IIR_MASK) >> MLX90615_CONFIG_IIR_SHIFT ] % 100) *
			10000;
		return IIO_VAL_INT_PLUS_MICRO;

	default:
		return -EINVAL;
	}
}

static int mlx90615_write_raw(struct iio_dev *indio_dev,
			     struct iio_chan_spec const *channel, int val,
			     int val2, long mask)
{
	struct mlx90615_data *data = iio_priv(indio_dev);
	s32 ret;

	switch (mask) {
	case IIO_CHAN_INFO_CALIBEMISSIVITY: /* 1/16384 / LSB */
		if (val < 0 || val2 < 0 || val > 1 || (val == 1 && val2 != 0))
			return -EINVAL;
		val = val * MLX90615_CONST_RAW_EMISSIVITY_MAX +
			val2 / MLX90615_CONST_EMISSIVITY_RESOLUTION;

		mutex_lock(&data->lock);
		ret = mlx90615_write_word(data->client, MLX90615_EMISSIVITY,
					  val);
		mutex_unlock(&data->lock);


		return ret;
	case IIO_CHAN_INFO_LOW_PASS_FILTER_3DB_FREQUENCY: /* IIR Filter setting */
		if ((val < 0) | (val > 1))
			return -EINVAL;


		mutex_lock(&data->lock);
		ret = mlx90615_iir_search(data->client,
					  val * 100 + val2 / 10000);
    /* write IIR */
		mutex_unlock(&data->lock);


		return ret;
	default:
		return -EINVAL;
	}
}




static int mlx90615_work_buffer(struct iio_dev *indio_dev)
{
	struct mlx90615_data *data = iio_priv(indio_dev);
	int ret, i = 0;
	s64 time;


	time = iio_get_time_ns(indio_dev);

	/*
	 * Single register reads: bulk_read will not work with ina226/219
	 * as there is no auto-increment of the register pointer.
	 */

        if (test_bit(0, indio_dev->active_scan_mask))
  	{

		ret = i2c_smbus_read_word_data(data->client, MLX90615_TA);
		if (ret < 0)
			return ret;
                if (ret & 0x8000)
                        return -EIO;
		data->scan.chan[0] = (unsigned int)ret;
                i++;
	}

        if (test_bit(1, indio_dev->active_scan_mask)) 
        {
                ret = i2c_smbus_read_word_data(data->client, MLX90615_TOBJ);
                if (ret < 0)
                        return ret;
                if (ret & 0x8000)
                        return -EIO;
                data->scan.chan[i] = (unsigned int)ret;
        }

	iio_push_to_buffers_with_timestamp(indio_dev, &data->scan, time);

	return 0;
};


static int mlx90615_capture_thread(void *data)
{
	struct iio_dev *indio_dev = data;
	int sampling_ms = 100;
	int ret, slept_ms=0;
	//struct timespec64 next, now, delta;
	//s64 delay_us;


	//ktime_get_ts64(&next);

	do {

                if (slept_ms == 0) {
		ret = mlx90615_work_buffer(indio_dev);
                slept_ms = msleep_interruptible(sampling_ms);
		if (ret < 0)
			return ret;
                }
                else {slept_ms = msleep_interruptible(slept_ms);}

		//ktime_get_ts64(&now);

		/*
		 * Advance the timestamp for the next poll by one sampling
		 * interval, and sleep for the remainder (next - now)
		 * In case "next" has already passed, the interval is added
		 * multiple times, i.e. samples are dropped.
		 */
		/* do {
			timespec64_add_ns(&next, 1000 * sampling_us);
			delta = timespec64_sub(next, now);
			delay_us = div_s64(timespec64_to_ns(&delta), 1000);
		} while (delay_us <= 0);

		usleep_range(delay_us, (delay_us * 3) >> 1);
                */


	} while (!kthread_should_stop());

	return 0;
}






static int mlx90615_buffer_enable(struct iio_dev *indio_dev)
{
	struct mlx90615_data *data = iio_priv(indio_dev);
	unsigned int sampling_ms = 100;
	struct task_struct *task;

	dev_info(&indio_dev->dev, "Enabling buffer w/ scan_mask %02x, freq = %d\n",
		(unsigned int)(*indio_dev->active_scan_mask),
		1000 / sampling_ms);

	task = kthread_create(mlx90615_capture_thread, (void *)indio_dev,
			      "%s:%d-%ums", indio_dev->name, indio_dev->id,
			      sampling_ms);
	if (IS_ERR(task))
		return PTR_ERR(task);

	get_task_struct(task);
	wake_up_process(task);
	data->task = task;

	return 0;
}


static int mlx90615_buffer_disable(struct iio_dev *indio_dev)
{
	struct mlx90615_data *data = iio_priv(indio_dev);
 dev_info(&indio_dev->dev, "Disabling buffer for MLX90615\n");


	if (data->task) {
		kthread_stop(data->task);
		put_task_struct(data->task);
		data->task = NULL;
	}

	return 0;
}


static const struct iio_buffer_setup_ops mlx90615_setup_ops = {
	.postenable = &mlx90615_buffer_enable,
	.predisable = &mlx90615_buffer_disable,
};


static int mlx90615_write_raw_get_fmt(struct iio_dev *indio_dev,
				     struct iio_chan_spec const *channel,
				     long mask)
{
	switch (mask) {
	case IIO_CHAN_INFO_CALIBEMISSIVITY:
		return IIO_VAL_INT_PLUS_NANO;
        case IIO_CHAN_INFO_LOW_PASS_FILTER_3DB_FREQUENCY:
		return IIO_VAL_INT_PLUS_MICRO;
	default:
		return -EINVAL;
	}
}

static const struct iio_chan_spec mlx90615_channels[] = {
	{
		.type = IIO_TEMP,
		.modified = 1,
                .indexed = 1,
		.channel2 = IIO_MOD_TEMP_AMBIENT,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
		.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_OFFSET) |
		    BIT(IIO_CHAN_INFO_SCALE),
                .scan_index = 0,
                .scan_type = {
                        .sign = 'u',
                        .realbits = 16,
                        .storagebits = 16,
                        .shift = 0,
                        .endianness = IIO_LE,
                },
	},
	{
		.type = IIO_TEMP,
		.modified = 1,
                .indexed = 1,
		.channel2 = IIO_MOD_TEMP_OBJECT,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |
		    BIT(IIO_CHAN_INFO_CALIBEMISSIVITY) |
		    BIT(IIO_CHAN_INFO_LOW_PASS_FILTER_3DB_FREQUENCY),
		.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_OFFSET) |
		    BIT(IIO_CHAN_INFO_SCALE),
                .scan_index = 1,
                .scan_type = {
                        .sign = 'u',
                        .realbits = 16,
                        .storagebits = 16,
                        .shift = 0,
                        .endianness = IIO_LE,
                },

	},
IIO_CHAN_SOFT_TIMESTAMP(2),
};

static const struct iio_info mlx90615_info = {
	.read_raw = mlx90615_read_raw,
	.write_raw = mlx90615_write_raw,
	.write_raw_get_fmt = mlx90615_write_raw_get_fmt,
	.attrs = &mlx90615_attr_group,

};





static int mlx90615_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct iio_dev *indio_dev;
	struct mlx90615_data *data;
        struct iio_buffer *buffer;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_WORD_DATA))
		return -ENODEV;

	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*data));
	if (!indio_dev)
		return -ENOMEM;

	data = iio_priv(indio_dev);
	i2c_set_clientdata(client, indio_dev);
	data->client = client;

	indio_dev->dev.parent = &client->dev;
	indio_dev->name = id->name;
	//indio_dev->modes = INDIO_DIRECT_MODE;
        indio_dev->modes = INDIO_DIRECT_MODE | INDIO_BUFFER_SOFTWARE;
	indio_dev->info = &mlx90615_info;
	indio_dev->channels = mlx90615_channels;
	indio_dev->num_channels = ARRAY_SIZE(mlx90615_channels);

        indio_dev->setup_ops = &mlx90615_setup_ops;


        buffer = devm_iio_kfifo_allocate(&indio_dev->dev);
	if (!buffer)
		return -ENOMEM;

	iio_device_attach_buffer(indio_dev, buffer);

	return iio_device_register(indio_dev);
}




static int mlx90615_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);

	iio_device_unregister(indio_dev);



	return 0;
}




static const struct i2c_device_id mlx90615_id[] = {
	{ "mlx90615", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mlx90615_id);




static struct i2c_driver mlx90615_driver = {
	.driver = {
		.name	= "mlx90615",
	},
	.probe = mlx90615_probe,
	.remove = mlx90615_remove,
	.id_table = mlx90615_id,
};
module_i2c_driver(mlx90615_driver);




MODULE_AUTHOR("LH <lh@shpi.de>");
MODULE_DESCRIPTION("Melexis MLX90615 contactless IR temperature sensor driver");
MODULE_LICENSE("GPL");
