#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>

#include "bt.h"
#include "ppg.h"

#define MSG_PERIOD_MS (1000U)
#define MSG_LEN (3U)

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static void bt_connected_cb(void);
static void send_tmr_cb(struct k_timer *p_tmr);

K_TIMER_DEFINE(send_tmr, send_tmr_cb, NULL);

int main(void)
{
	// bt_start(bt_connected_cb);
	bt_connected_cb();

	return 0;
}

static void bt_connected_cb(void)
{
	ppg_start_sampling();
	k_timer_start(&send_tmr, K_MSEC(MSG_PERIOD_MS), K_MSEC(MSG_PERIOD_MS));
}

static void send_tmr_cb(struct k_timer *p_tmr)
{
	uint8_t msg[MSG_LEN];
	uint16_t rmssd;

	msg[0] = (uint8_t) ppg_get_hr_bpm();
	rmssd = (uint16_t) ppg_get_rmssd();
	msg[1] = (uint8_t) rmssd;
	msg[2] = (uint8_t) (rmssd >> 8);


	LOG_DBG("Sending message: HR %d, RMSSD %d", msg[0], (msg[2] << 8) | msg[1]);

	// bt_send_notification(msg, MSG_LEN);
}
