#include "eda.h"

#include <zephyr/logging/log.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>

#define SAMPLE_PERIOD_MS 20 // 50 Hz

LOG_MODULE_REGISTER(eda, LOG_LEVEL_DBG);

static void sampling_tmr_cb(struct k_timer *p_tmr);

static const struct adc_dt_spec adc_channel = ADC_DT_SPEC_GET(DT_PATH(zephyr_user));

static int16_t adc_buf;
static struct adc_sequence adc_seq = {
    .buffer = &adc_buf,
    .buffer_size = sizeof(adc_buf),
};

static K_TIMER_DEFINE(sampling_tmr, sampling_tmr_cb, NULL);

int eda_start_sampling(void)
{
    int err;

    if (!adc_is_ready_dt(&adc_channel))
    {
	    LOG_ERR("ADC not ready");
	    return -1;
    }

    err = adc_channel_setup_dt(&adc_channel);
    if (err < 0)
    {
	    LOG_ERR("Could not setup ADC channel (%d)", err);
	    return err;
    }

    err = adc_sequence_init_dt(&adc_channel, &adc_seq);
    if (err < 0)
    {
	    LOG_ERR("Could not initialize sequence (%d)", err);
	    return err;
    }

    k_timer_start(&sampling_tmr, K_MSEC(SAMPLE_PERIOD_MS), K_MSEC(SAMPLE_PERIOD_MS));

    return 0;
}

static void sampling_tmr_cb(struct k_timer *p_tmr)
{
    int err;
    int32_t mv;

    err = adc_read_dt(&adc_channel, &adc_seq);
    if (err < 0)
    {
        LOG_ERR("Error reading from ADC");
    }
    else
    {
        mv = adc_buf;
        adc_raw_to_millivolts_dt(&adc_channel, &mv);
        LOG_DBG("ADC got value %d (%d mV)", adc_buf, mv);
    }
}
