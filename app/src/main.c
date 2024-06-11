#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>

#include "bt.h"
#include "ppg.h"
#include "eda.h"

#define MSG_PERIOD_MS (1000U)
#define MSG_LEN (7U)

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static void bt_connected_cb(void);
static void send_tmr_cb(struct k_timer *p_tmr);

K_TIMER_DEFINE(send_tmr, send_tmr_cb, NULL);

int main(void)
{
	bt_start(bt_connected_cb);
	
	return 0;
}

static void bt_connected_cb(void)
{
	ppg_start_sampling();
	eda_start_sampling();
	k_timer_start(&send_tmr, K_MSEC(MSG_PERIOD_MS), K_MSEC(MSG_PERIOD_MS));
}

static void send_tmr_cb(struct k_timer *p_tmr)
{
	uint8_t msg[MSG_LEN];
	uint16_t rmssd;
	uint16_t ppg_ampl;
	uint16_t epc;

	/**
	 * Message bytes:
	 * [0] HR
	 * [1] RMSSD low byte
	 * [2] RMSSD high byte
	 * [3] PPG amplitude low byte
	 * [4] PPG amplitude high byte
	 * [5] EPC low byte
	 * [6] EPC high byte
	*/

	msg[0] = (uint8_t) ppg_get_hr_bpm();

	rmssd = (uint16_t) ppg_get_rmssd();
	msg[1] = (uint8_t) (rmssd & 0xFF);
	msg[2] = (uint8_t) (rmssd >> 8);

	ppg_ampl = (uint16_t) ppg_get_amplitude();
	msg[3] = (uint8_t) (ppg_ampl & 0xFF);
	msg[4] = (uint8_t) (ppg_ampl >> 8);

	epc = (uint16_t) eda_get_epc();
	msg[5] = (uint8_t) (epc & 0xFF);
	msg[6] = (uint8_t) (epc >> 8);

	LOG_DBG("Sending message: HR %d, RMSSD %d, PPG ampl %d, EPC %d",
			msg[0],
			((uint16_t) msg[2] << 8) | msg[1],
			((uint16_t) msg[4] << 8) | msg[3],
			((uint16_t) msg[5] << 8) | msg[6]);

	bt_send_notification(msg, MSG_LEN);
}
