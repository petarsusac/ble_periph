#include "ppg.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <math.h>

#include "heartRate.h"
#include "ring_buffer.h"

#define SAMPLE_PERIOD_MS 20 // 50 Hz
#define HR_MOV_AVG_SIZE 4
#define IBI_MOV_AVG_SIZE 30
#define AMP_MOV_AVG_SIZE 4

LOG_MODULE_REGISTER(ppg, CONFIG_APP_LOG_LEVEL);

static void sampling_tmr_cb(struct k_timer *p_tmr);
static inline float ms_to_bpm(int64_t ms);

static K_TIMER_DEFINE(sampling_tmr, sampling_tmr_cb, NULL);

static float hr_mov_avg_buf[HR_MOV_AVG_SIZE];
static ring_buffer_t hr_mov_avg_ring_buf;

static float ibi_mov_avg_buf[IBI_MOV_AVG_SIZE];
static ring_buffer_t ibi_mov_avg_ring_buf;

static float amp_mov_avg_buf[AMP_MOV_AVG_SIZE];
static ring_buffer_t amp_mov_avg_ring_buf;

// Testing only
static const int32_t test_samples[300] = {35115, 35119, 35113, 35107, 35107, 35111, 35109, 35114, 35118, 35130, 35146, 35155, 35169, 35176, 35190, 35197, 35217, 35237, 35251, 35263, 35265, 35225, 35124, 35065, 35019, 34998, 34989, 34991, 34997, 35007, 35014, 35026, 35045, 35054, 35073, 35074, 35081, 35076, 35066, 35064, 35063, 35067, 35068, 35076, 35088, 35096, 35103, 35117, 35129, 35142, 35149, 35166, 35181, 35202, 35204, 35183, 35099, 35026, 34987, 34959, 34937, 34936, 34936, 34951, 34963, 34970, 34985, 34994, 35007, 35013, 35014, 35019, 35009, 35013, 35010, 35009, 35012, 35022, 35037, 35041, 35051, 35066, 35073, 35089, 35105, 35118, 35143, 35167, 35176, 35184, 35136, 35028, 34962, 34916, 34900, 34885, 34888, 34895, 34904, 34906, 34913, 34915, 34932, 34940, 34959, 34961, 34965, 34968, 34960, 34955, 34955, 34957, 34966, 34980, 34996, 35008, 35023, 35035, 35046, 35058, 35077, 35088, 35104, 35129, 35141, 35147, 35106, 35001, 34930, 34882, 34860, 34849, 34844, 34846, 34859, 34867, 34871, 34887, 34898, 34910, 34922, 34931, 34936, 34934, 34930, 34926, 34924, 34927, 34936, 34953, 34971, 34979, 35004, 35018, 35033, 35046, 35058, 35066, 35088, 35101, 35121, 35143, 35157, 35153, 35089, 34962, 34895, 34859, 34839, 34834, 34831, 34844, 34863, 34877, 34882, 34893, 34905, 34922, 34930, 34938, 34933, 34933, 34923, 34925, 34919, 34927, 34941, 34953, 34965, 34978, 34996, 35008, 35027, 35038, 35054, 35072, 35091, 35105, 35124, 35118, 35057, 34937, 34871, 34835, 34818, 34815, 34820, 34832, 34845, 34857, 34869, 34884, 34900, 34916, 34927, 34919, 34920, 34914, 34910, 34910, 34914, 34927, 34934, 34948, 34963, 34979, 34999, 35008, 35028, 35050, 35063, 35082, 35109, 35129, 35122, 35054, 34926, 34859, 34820, 34794, 34785, 34787, 34800, 34808, 34820, 34831, 34841, 34853, 34869, 34881, 34888, 34883, 34879, 34879, 34877, 34881, 34892, 34897, 34909, 34917, 34935, 34958, 34974, 34987, 35007, 35021, 35041, 35059, 35081, 35087, 35057, 34940, 34848, 34802, 34776, 34759, 34761, 34768, 34777, 34786, 34792, 34802, 34818, 34829, 34842, 34855, 34852, 34854, 34851, 34845, 34851, 34848, 34857, 34865, 34883, 34896, 34921, 34941, 34949, 34967, };

void ppg_start_sampling(void)
{
    ring_buffer_init(&hr_mov_avg_ring_buf, hr_mov_avg_buf, HR_MOV_AVG_SIZE);
    ring_buffer_init(&ibi_mov_avg_ring_buf, ibi_mov_avg_buf, IBI_MOV_AVG_SIZE);
    ring_buffer_init(&amp_mov_avg_ring_buf, amp_mov_avg_buf, AMP_MOV_AVG_SIZE);
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

static void sampling_tmr_cb(struct k_timer *p_tmr)
{
    // Testing only
    static int i = 0;
    int32_t new_sample = test_samples[i];
    i = (i + 1) % 300;

    static int64_t last_beat_ms = 0;
    int64_t current_beat_ms;
    int64_t diff_ms;
    float bpm;
    int16_t amplitude;

    if (checkForBeat(new_sample, &amplitude))
    {
        current_beat_ms = k_uptime_get();

        if (last_beat_ms != 0)
        {
            diff_ms = current_beat_ms - last_beat_ms;

            bpm = ms_to_bpm(diff_ms);
            ring_buffer_put(&hr_mov_avg_ring_buf, bpm);

            ring_buffer_put(&ibi_mov_avg_ring_buf, (float) diff_ms);

            ring_buffer_put(&amp_mov_avg_ring_buf, (float) amplitude);
        }

        last_beat_ms = current_beat_ms;
    }
}

static inline float ms_to_bpm(int64_t ms)
{
    return 60.f / (ms / 1000.f);
}
