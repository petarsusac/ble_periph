#define DT_DRV_COMPAT maxim_max30100

#include "max30100.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(MAX30100, CONFIG_SENSOR_LOG_LEVEL);

static int max30100_sample_fetch(const struct device *dev,
				 enum sensor_channel chan)
{
	const struct max30100_config *config = dev->config;
	struct max30100_data *data = dev->data;

	uint8_t buffer[MAX30100_BYTES_PER_SAMPLE];

	if (i2c_burst_read_dt(&config->i2c, MAX30100_REG_FIFO_DATA, buffer,
			      MAX30100_BYTES_PER_SAMPLE)) {
		LOG_ERR("Could not fetch sample");
		return -EIO;
	}

	data->ir = ((uint16_t) buffer[0] << 8) | buffer[1];
	data->red = ((uint16_t) buffer[2] << 8) | buffer[3];

    return 0;
}

static int max30100_channel_get(const struct device *dev,
				enum sensor_channel chan,
				struct sensor_value *val)
{
	const struct max30100_config *config = dev->config;
	struct max30100_data *data = dev->data;

	switch(chan)
	{
		case SENSOR_CHAN_RED:
			val->val1 = data->red;
			val->val2 = 0;
		break;

		case SENSOR_CHAN_IR:
			if (MAX30100_MODE_SPO2 == config->mode)
			{
				val->val1 = data->ir;
				val->val2 = 0;
			}
			else
			{
				LOG_ERR("Attempted to read IR channel but device is not in Sp02 mode");
				return -ENOTSUP;
			}
		break;

		default:
			LOG_ERR("Channel not supported");
			return -ENOTSUP;
		break;
	}

    return 0;
}

static int max30100_init(const struct device *dev)
{
    const struct max30100_config *config = dev->config;
    struct max30100_data *data = dev->data;
    uint8_t part_id;
    uint8_t mode_cfg;

    if (!device_is_ready(config->i2c.bus)) {
		LOG_ERR("Bus device is not ready");
		return -ENODEV;
	}

    /* Check the part id to make sure this is MAX30101 */
	if (i2c_reg_read_byte_dt(&config->i2c, MAX30100_REG_PART_ID,
				 &part_id)) {
		LOG_ERR("Could not get Part ID");
		return -EIO;
	}

    if (part_id != MAX30100_PART_ID) {
		LOG_ERR("Got Part ID 0x%02x, expected 0x%02x",
			    part_id, MAX30100_PART_ID);
		return -EIO;
	}

    /* Reset the sensor */
	if (i2c_reg_write_byte_dt(&config->i2c, MAX30100_REG_MODE_CFG,
				  MAX30100_MODE_CFG_RESET_MASK)) {
		return -EIO;
	}

    /* Wait for reset to be cleared */
	do {
		if (i2c_reg_read_byte_dt(&config->i2c, MAX30100_REG_MODE_CFG,
					 &mode_cfg)) {
			LOG_ERR("Could not read mode cfg after reset");
			return -EIO;
		}
	} while (mode_cfg & MAX30100_MODE_CFG_RESET_MASK);

    /* Write the mode configuration register */
	if (i2c_reg_write_byte_dt(&config->i2c, MAX30100_REG_MODE_CFG,
				  config->mode)) {
		return -EIO;
	}

    /* Write the SpO2 configuration register */
	if (i2c_reg_write_byte_dt(&config->i2c, MAX30100_REG_SPO2_CFG,
				  config->spo2)) {
		return -EIO;
	}

	/* Write the LED configuration register */
	if (i2c_reg_write_byte_dt(&config->i2c, MAX30100_REG_LED_CFG,
				  config->led)) {
		return -EIO;
	}

    LOG_DBG("Successful init");

    return 0;
}

static const struct sensor_driver_api max30100_driver_api = {
	.sample_fetch = max30100_sample_fetch,
	.channel_get = max30100_channel_get,
};

static const struct max30100_config max30100_config = {
    .i2c = I2C_DT_SPEC_INST_GET(0),
    .mode = MAX30100_MODE_SPO2,
    .spo2 = MAX30100_SPO2_CFG_SR
            | MAX30100_SPO2_CFG_LED_PW,
    .led = MAX30100_LED_CFG_IR | MAX30100_LED_CFG_RED,
};

static struct max30100_data max30100_data;

SENSOR_DEVICE_DT_INST_DEFINE(0, max30100_init, NULL,
		    &max30100_data, &max30100_config,
		    POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY,
		    &max30100_driver_api);