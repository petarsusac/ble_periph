#ifndef _PPG_H_
#define _PPG_H_

#include <stdint.h>

void ppg_start_sampling(void);
uint32_t ppg_get_hr_bpm(void);
uint32_t ppg_get_rmssd(void);

#endif /* _PPG_H_ */