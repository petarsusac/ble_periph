#ifndef _PPG_H_
#define _PPG_H_

#include <stdint.h>

int ppg_init(void);
void ppg_start_sampling(void);
uint32_t ppg_get_hr_bpm(void);
uint32_t ppg_get_rmssd(void);
uint32_t ppg_get_amplitude(void);

#endif /* _PPG_H_ */