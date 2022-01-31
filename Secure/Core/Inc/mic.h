/*
 * mic.h
 *
 *  Created on: Oct 9, 2021
 *      Author: AUTHOR_NAME_REMOVED
 */

#ifndef INC_MIC_H_
#define INC_MIC_H_

#include "main.h"

#define MIC_BUFFER_LEN 1024

struct mic_memory_zone_t {
	uint32_t chaf;
	int16_t *active_buffer;
	uint16_t new_data;
	int16_t buffer_a[MIC_BUFFER_LEN];
	int16_t buffer_b[MIC_BUFFER_LEN];
};

static __attribute__((unused)) struct mic_memory_zone_t *mic_buffer;

uint32_t __attribute__((cmse_nonsecure_entry)) mic_check_for_new_data(void);

void mic_set_buffer(struct mic_memory_zone_t * location);
void mic_start_stream(SAI_HandleTypeDef *sai);
int16_t * mic_get_application_buffer(void);

#endif /* INC_MIC_H_ */
