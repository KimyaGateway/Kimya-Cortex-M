/*
 * audio.h
 *
 *  Created on: Oct 8, 2021
 *      Author: AUTHOR_NAME_REMOVED
 */

#ifndef INC_AUDIO_H_
#define INC_AUDIO_H_

#include "arm_math.h"
#include "speechparams.h"

#define AUDIO_MEL_ROLING_BUFFER_SIZE 15

struct audio_acquire_heap_t {
	uint32_t mel_count;
	uint32_t mel_cursor;
	float32_t mels[AUDIO_MEL_ROLING_BUFFER_SIZE][SP_MEL_NUM_BINS];
	arm_rfft_fast_instance_f32 rfft_f32;
	float32_t signal_buffer[SP_FRAME_LEN];
	float32_t fft_buffer[SP_FRAME_LEN];
	float32_t pspectrum_buffer[SP_FRAME_LEN/2];
};

void audio_calc_init(struct audio_acquire_heap_t *buffers);
void audio_calc_mel_coefficients(const int16_t *input, struct audio_acquire_heap_t *buffers);

#endif /* INC_AUDIO_H_ */
