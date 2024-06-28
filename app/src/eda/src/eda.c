#include "eda.h"

#include <zephyr/logging/log.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>

#include <math.h>

#include "ring_buffer.h"

#define SAMPLE_PERIOD_MS 10 // 100 Hz

#define SMPL_THRD_STACK_SIZE (8192U)
#define SMPL_THRD_PRIO 2

#define OVERSAMPLING_BUF_SIZE 10 // 10 Hz after averaging
#define EDA_BUF_SIZE 50

#define NUM_FILT_COEFS 13

LOG_MODULE_REGISTER(eda, CONFIG_APP_LOG_LEVEL);

static void sampling_tmr_cb(struct k_timer *p_tmr);
static void eda_smpl_thrd_run(void *p1, void *p2, void *p3);
static inline float mv_to_eda_ns(float mv);

static K_THREAD_DEFINE(eda_smpl_thrd,
                       8192,
                       eda_smpl_thrd_run,
                       NULL, NULL, NULL, 
                       SMPL_THRD_PRIO, 0, 0);

static K_SEM_DEFINE(sampling_sem, 0, 1);

static K_TIMER_DEFINE(sampling_tmr, sampling_tmr_cb, NULL);

static const struct adc_dt_spec adc_channel = ADC_DT_SPEC_GET(DT_PATH(zephyr_user));

static uint16_t adc_buf;
static struct adc_sequence adc_seq = {
    .buffer = &adc_buf,
    .buffer_size = sizeof(adc_buf),
};

static int32_t oversampling_buf[OVERSAMPLING_BUF_SIZE];
static size_t oversampling_buf_idx;

static float eda_buf[EDA_BUF_SIZE];
static ring_buffer_t eda_ring_buf;

static const float filt_coefs[NUM_FILT_COEFS] = {
    -0.002719748296486632f,
    1.2038218586409178e-18f,
    0.01580853697332853f,
    0.05940870999155988f,
    0.12706862970416977f,
    0.19141010953237056f,
    0.21804752419011558f,
    0.19141010953237056f,
    0.12706862970416982f,
    0.05940870999155988f,
    0.01580853697332855f,
    1.2038218586409182e-18f,
    -0.002719748296486632f, 
};

static int32_t filt_sample_buf[NUM_FILT_COEFS];

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

    memset(oversampling_buf, 0, OVERSAMPLING_BUF_SIZE * sizeof(int32_t));
    oversampling_buf_idx = 0;
    memset(filt_sample_buf, 0, NUM_FILT_COEFS * sizeof(int32_t));

    err = ring_buffer_init(&eda_ring_buf, eda_buf, EDA_BUF_SIZE);
    if (err < 0)
    {
        LOG_ERR("Failed to init EDA ring buffer");
        return err;
    }

    k_timer_start(&sampling_tmr, K_MSEC(SAMPLE_PERIOD_MS), K_MSEC(SAMPLE_PERIOD_MS));

    return 0;
}

uint32_t eda_get_epc(void)
{
    float epc = 0.0f;
    float eda_samples_buf[EDA_BUF_SIZE];
    int num_items;

    if (ring_buffer_get_num_items(&eda_ring_buf) > 1)
    {
        num_items = ring_buffer_copy_inorder(&eda_ring_buf, eda_samples_buf);
        
        for(int i = 1; i < num_items; i++)
        {
            if (eda_samples_buf[i] > eda_samples_buf[i-1])
            {
                epc += eda_samples_buf[i] - eda_samples_buf[i-1];
            }
        }
    }

    return (uint32_t) roundf(epc);
}

static void sampling_tmr_cb(struct k_timer *p_tmr)
{
    k_sem_give(&sampling_sem);
}

static void eda_smpl_thrd_run(void *p1, void *p2, void *p3)
{
    int err;
    int32_t mv;
    int32_t avg_mv;
    float filt_sum;
    float eda_value_ns;

    for(;;)
    {
        k_sem_take(&sampling_sem, K_FOREVER);

        err = adc_read_dt(&adc_channel, &adc_seq);
        if (err < 0)
        {
            LOG_ERR("Error reading from ADC");
        }
        else
        {
            // HACK! For some reason the ADC readings are offset by 1023, possible
            // bug in the ESP32 ADC driver
            if (adc_buf >= 1023)
            {
                adc_buf = adc_buf - 1023;
            }
            else
            {
                adc_buf = 0;
            }
            
            mv = adc_buf;
            adc_raw_to_millivolts_dt(&adc_channel, &mv);

            oversampling_buf[oversampling_buf_idx++] = mv;
            if (oversampling_buf_idx >= OVERSAMPLING_BUF_SIZE)
            {
                oversampling_buf_idx = 0;
                avg_mv = 0;
                for (size_t i = 0; i < OVERSAMPLING_BUF_SIZE; i++)
                {
                    avg_mv += oversampling_buf[i];
                }

                avg_mv /= OVERSAMPLING_BUF_SIZE;

                memmove(filt_sample_buf, &filt_sample_buf[1], (NUM_FILT_COEFS - 1) * sizeof(int32_t));
                filt_sample_buf[NUM_FILT_COEFS - 1] = avg_mv;
                filt_sum = 0.0f;

                for (size_t i = 0; i < NUM_FILT_COEFS; i++)
                {
                    filt_sum += filt_coefs[i] * filt_sample_buf[i];
                }

                // printk("%d\n", (int) filt_sum);

                eda_value_ns = mv_to_eda_ns(filt_sum);

                ring_buffer_put(&eda_ring_buf, eda_value_ns);
            }
        }
    }
}

// Convert value from ADC in mV to skin conductance value in nS
static inline float mv_to_eda_ns(float mv)
{
    const float r_403 = 200000.0f;
    const float a_u_ref = 0.995f;
    float rs = r_403 * ((a_u_ref / (mv / 1000.0f)) - 1.0f);
    return (1.0e9f / rs);
}
