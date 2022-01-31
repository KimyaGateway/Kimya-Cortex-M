/*
 * audio.c
 *
 *  Created on: Oct 8, 2021
 *      Author: AUTHOR_NAME_REMOVED
 */

#include "audio.h"
#include "speechparams.h"
#include "arm_math.h"

#define AUDIO_MEL_ROLING_BUFFER_SIZE 15

void audio_calc_init(struct audio_acquire_heap_t *buffers){
	arm_status status = arm_rfft_fast_init_f32(&buffers->rfft_f32, SP_FRAME_LEN);
	while (status != ARM_MATH_SUCCESS) {};
}

void audio_calc_mel_coefficients(const int16_t *input, struct audio_acquire_heap_t *heap){

	// First convert input to floats
	arm_q15_to_float(input, heap->signal_buffer, SP_FRAME_LEN);

	// perform FFT, the function we use only gives half of the spectrum
	arm_rfft_fast_f32(&heap->rfft_f32, heap->signal_buffer, heap->fft_buffer, 0);

	// Now, calculate complex magnitude,
	arm_cmplx_mag_f32(heap->fft_buffer, heap->pspectrum_buffer, SP_FRAME_LEN/2);

	float32_t *mel_buffer = (float32_t *) &(heap->mels[heap->mel_cursor]);
	// Sum magnitudes in to mel buckets
	for (uint16_t mel_index = 0; mel_index < SP_MEL_NUM_BINS; mel_index++){
		float32_t *bucket = &mel_buffer[mel_index];
		*bucket = 0;

		float32_t *start_freq = &(heap->pspectrum_buffer[sp_mel_points[mel_index]]);
		float32_t *end_freq = &(heap->pspectrum_buffer[sp_mel_points[mel_index + 1]]);

		for(float32_t *freq_cursor = start_freq; freq_cursor < end_freq; freq_cursor++){
			*bucket += *freq_cursor;
		}
	}
	heap->mel_cursor = (heap->mel_cursor + 1) % AUDIO_MEL_ROLING_BUFFER_SIZE;
}


