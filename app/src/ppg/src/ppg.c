#include "ppg.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/devicetree.h>
#include <math.h>

#include "heartRate.h"
#include "ring_buffer.h"

#define SMPL_THRD_STACK_SIZE (8192U)
#define SMPL_THRD_PRIO 3

#define SAMPLE_PERIOD_MS 20 // 50 Hz

#define HR_MOV_AVG_SIZE 4
#define IBI_MOV_AVG_SIZE 30
#define AMP_MOV_AVG_SIZE 4

#define HR_MIN 40.0f
#define HR_MAX 200.0f

LOG_MODULE_REGISTER(ppg, CONFIG_APP_LOG_LEVEL);

static void ppg_smpl_thrd_run(void *p1, void *p2, void *p3);
static inline float ms_to_bpm(int64_t ms);

static K_THREAD_DEFINE(ppg_smpl_thrd,
                       8192,
                       ppg_smpl_thrd_run,
                       NULL, NULL, NULL, 
                       SMPL_THRD_PRIO, 0, 0);

static K_TIMER_DEFINE(sampling_tmr, NULL, NULL);

static float hr_mov_avg_buf[HR_MOV_AVG_SIZE];
static ring_buffer_t hr_mov_avg_ring_buf;

static float ibi_mov_avg_buf[IBI_MOV_AVG_SIZE];
static ring_buffer_t ibi_mov_avg_ring_buf;

static float amp_mov_avg_buf[AMP_MOV_AVG_SIZE];
static ring_buffer_t amp_mov_avg_ring_buf;

// static const struct device *const p_sensor_dev = DEVICE_DT_GET(DT_NODELABEL(max30101));
static const struct device *const p_sensor_dev = DEVICE_DT_GET(DT_NODELABEL(max30100));

int ppg_init(void)
{
    int err;

    if (!device_is_ready(p_sensor_dev))
    {
        LOG_ERR("PPG sensor not ready");
        return -1;
    }

    err = ring_buffer_init(&hr_mov_avg_ring_buf, hr_mov_avg_buf, HR_MOV_AVG_SIZE);
    if (0 == err)
    {
        err = ring_buffer_init(&ibi_mov_avg_ring_buf, ibi_mov_avg_buf, IBI_MOV_AVG_SIZE);
    }
    if (0 == err)
    {
        err = ring_buffer_init(&amp_mov_avg_ring_buf, amp_mov_avg_buf, AMP_MOV_AVG_SIZE);
    }

    return err;
}

void ppg_start_sampling(void)
{
    k_timer_start(&sampling_tmr, K_MSEC(SAMPLE_PERIOD_MS), K_MSEC(SAMPLE_PERIOD_MS));
}

uint32_t ppg_get_hr_bpm(void)
{
    float bpm;
    ring_buffer_mov_avg(&hr_mov_avg_ring_buf, &bpm);
    return (uint32_t) roundf(bpm);
}

uint32_t ppg_get_rmssd(void)
{
    float ibi_buf[IBI_MOV_AVG_SIZE];
    int num_ibi;
    float ibi_diff;
    float ibi_diff_sq_sum = 0.0f;
    float rmssd;

    if (ring_buffer_get_num_items(&ibi_mov_avg_ring_buf) > 1)
    {
        num_ibi = ring_buffer_copy_inorder(&ibi_mov_avg_ring_buf, ibi_buf);
        
        for(size_t i = 1; i < num_ibi; i++)
        {
            ibi_diff = ibi_buf[i] - ibi_buf[i-1];
            ibi_diff_sq_sum += ibi_diff * ibi_diff;
        }

        rmssd = sqrtf(ibi_diff_sq_sum / (num_ibi - 1));
    }
    else
    {
        rmssd = 0.0f;
    }

    return (uint32_t) roundf(rmssd);
}

uint32_t ppg_get_amplitude(void)
{
    float ampl;
    ring_buffer_mov_avg(&amp_mov_avg_ring_buf, &ampl);
    return (uint32_t) roundf(ampl);
}

static void ppg_smpl_thrd_run(void *p1, void *p2, void *p3)
{
    int ret;
    struct sensor_value sens_val;
    static int64_t last_beat_ms = 0;
    int64_t current_beat_ms;
    int64_t diff_ms;
    float bpm;
    int16_t amplitude;

    for (;;)
    {
        k_timer_status_sync(&sampling_tmr); 

        ret = sensor_sample_fetch_chan(p_sensor_dev, SENSOR_CHAN_RED);
        if (ret != 0)
        {
            LOG_ERR("Failed to fetch PPG sample");
            return;
        }

        ret = sensor_channel_get(p_sensor_dev, SENSOR_CHAN_RED, &sens_val);
        if (ret != 0)
        {
            LOG_ERR("Failed to get PPG sample");
            return;
        }

        // printk("%d\n", sens_val.val1);

        if (checkForBeat(sens_val.val1, &amplitude))
        {
            current_beat_ms = k_uptime_get();

            if (last_beat_ms != 0)
            {
                diff_ms = current_beat_ms - last_beat_ms;

                bpm = ms_to_bpm(diff_ms);

                // Check if HR is realistic to reduce the effect of missed or
                // false heart beats
                if ((bpm > HR_MIN) && (bpm < HR_MAX))
                {
                    ring_buffer_put(&hr_mov_avg_ring_buf, bpm);

                    ring_buffer_put(&ibi_mov_avg_ring_buf, (float) diff_ms);

                    ring_buffer_put(&amp_mov_avg_ring_buf, (float) amplitude);

                    // printk("%d\n", (int) bpm);
                }
            }

            last_beat_ms = current_beat_ms;
        }
    }
}

static inline float ms_to_bpm(int64_t ms)
{
    return 60.f / (ms / 1000.f);
}
