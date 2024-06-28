#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/util.h>

#define MAX30100_REG_INT_STAT   0x00
#define MAX30100_REG_INT_EN     0x01

#define MAX30100_REG FIFO_WR    0x02
#define MAX30100_REG_FIFO_OVF   0x03
#define MAX30100_REG_FIFO_RD    0x04
#define MAX30100_REG_FIFO_DATA  0x05

#define MAX30100_REG_MODE_CFG   0x06
#define MAX30100_REG_SPO2_CFG   0x07
#define MAX30100_REG_LED_CFG    0x09

#define MAX30100_REG_PART_ID    0xFF

#define MAX30100_PART_ID        0x11

#define MAX30100_MODE_CFG_RESET_MASK    BIT(6)

#define MAX30100_SPO2_CFG_HI_RES_EN BIT(6)
#define MAX30100_SPO2_CFG_SR        0x0
#define MAX30100_SPO2_CFG_LED_PW    0x3

#define MAX30100_LED_CFG_IR     0x6
#define MAX30100_LED_CFG_RED    (0x6 << 4)

#define MAX30100_BYTES_PER_SAMPLE   4

enum max30100_mode {
	MAX30100_MODE_HEART_RATE    = 2,
	MAX30100_MODE_SPO2		    = 3,
};

struct max30100_config {
    struct i2c_dt_spec i2c;
    enum max30100_mode mode;
    uint8_t spo2;
    uint8_t led;
};

struct max30100_data {
    uint16_t red;
    uint16_t ir;
};
