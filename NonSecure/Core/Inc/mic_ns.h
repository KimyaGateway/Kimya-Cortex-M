/*
 * mic_ns.h
 *
 *  Created on: Oct 12, 2021
 *      Author: AUTHOR_NAME_REMOVED
 */

#ifndef INC_MIC_NS_H_
#define INC_MIC_NS_H_

/* NSC function */
uint32_t mic_check_for_new_data(void);

#define MIC_BUFFER_LEN 1024
#define MIC_BUFFER_LOCATION 0x2003c000

struct mic_memory_zone_t {
	uint32_t chaf;
	int16_t *active_buffer;
	uint16_t new_data;
	int16_t buffer_a[MIC_BUFFER_LEN];
	int16_t buffer_b[MIC_BUFFER_LEN];
};

#endif /* INC_MIC_NS_H_ */
