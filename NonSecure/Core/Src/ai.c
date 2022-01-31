/*
 * ai.c
 *
 *  Created on: Oct 8, 2021
 *      Author: AUTHOR_NAME_REMOVED
 */

#include "ai.h"
#include "network.h"
#include "network_data.h"
#include "speechparams.h"
#include "arm_math.h"
#include "network_img_rel.h"
#include <string.h>
//#include "printf.h"

#define printf(...)

/*
 * Initialize the relocatable neural network
 */
void ai_init(struct ai_heap_t *heap) {
	ai_error err;
	ai_rel_network_info rt_info;

	ai_handle binary_image_address = ai_network_reloc_img_get();

	/* 1 - Get and Print some info about the network */
	err = ai_rel_network_rt_get_info(binary_image_address, &rt_info);
	if (err.type != AI_ERROR_NONE) {
		printf("E: AI ai_rel_network_rt_get_info error - type=%d code=%d\r\n",
				err.type, err.code);
		while (1)
			;
	};

	printf("Load a relocatable binary model, located at the address 0x%08x\r\n",
			(int) binary_image_address);
	printf(" model name                : %s\r\n", rt_info.c_name);
	printf(" weights size              : %d bytes\r\n",
			(int) rt_info.weights_sz);
	printf(" weights address           : 0x%x \r\n",
			(int) rt_info.weights);
	printf(" activations size          : %d bytes (minimum)\r\n",
			(int) rt_info.acts_sz);
	printf(" compiled for a Cortex-Mx  : 0x%03X\r\n",
			(int) AI_RELOC_RT_GET_CPUID(rt_info.variant));
	printf(" FPU should be enabled     : %s\r\n",
			AI_RELOC_RT_FPU_USED(rt_info.variant) ? "yes" : "no");
	printf(" RT RAM minimum size       : %d bytes (%d bytes in COPY mode)\r\n",
			(int) rt_info.rt_ram_xip, (int) rt_info.rt_ram_copy);

	/* 2 - Load the network and create an instance */
	err = ai_rel_network_load_and_create(binary_image_address, heap->rt_ai_ram,
			rt_info.rt_ram_xip,
			AI_RELOC_RT_LOAD_MODE_XIP, &heap->network);
	if (err.type != AI_ERROR_NONE) {
		printf(
				"E: AI ai_rel_network_load_and_create error - type=%d code=%d\r\n",
				err.type, err.code);
		while (1)
			;
	};

	/* 3 - Initialize the instance */
	if (!ai_rel_network_init(heap->network, rt_info.weights, heap->activations)) {
		err = ai_network_get_error(heap->network);
		printf("E: AI ai_rel_network_init error - type=%d code=%d\r\n", err.type,
				err.code);
		while (1)
			;
	}
	printf("Network fully initialized\n");
}

/*
 * Run an interference on the network
 */
void ai_run(const void *in_data, void *out_data, struct ai_heap_t *heap)
{
  ai_i32 n_batch;
  ai_error __attribute__((unused)) err;

  /* 1 - Create the AI buffer IO handlers with the default definition */
  ai_buffer ai_input[AI_NETWORK_IN_NUM] = AI_NETWORK_IN ;
  ai_buffer ai_output[AI_NETWORK_OUT_NUM] = AI_NETWORK_OUT ;

  /* 2 - Update IO handlers with the data payload */
  ai_input[0].n_batches = 1;
  ai_input[0].data = AI_HANDLE_PTR(in_data);
  ai_output[0].n_batches = 1;
  ai_output[0].data = AI_HANDLE_PTR(out_data);

  /* 3 - Perform the inference */
  n_batch = ai_rel_network_run(heap->network, &ai_input[0], &ai_output[0]);
  if (n_batch != 1) {
      err = ai_network_get_error(heap->network);
//      printf("E: AI ai_network_run error - type=%d code=%d\r\n", err.type, err.code);
      while(1);
  };

  return;
}

/*
 * Take the mels from the acquire heaps, and place in in the NN input buffer.
 * Returns 0 if not yet enough audio frames have been captured, 1 otherwise.
 */
uint32_t ai_prepare_mels(struct audio_acquire_heap_t *active_acquire_heap,
		struct audio_acquire_heap_t *inactive_acquire_heap, struct ai_heap_t *heap){

	uint32_t total_required_mels = AI_INPUT_MEL_FRAMES;

	/* if there is not yet a full NN frame worth of data, wait. */
	if (active_acquire_heap->mel_count + inactive_acquire_heap->mel_count < total_required_mels){
		return 0;
	}

	ai_float *ai_in_cursor = heap->ai_in_data;

	/* First grab the required mel frames from the inactive acquire heap	*/
	uint32_t inactive_heap_required_mels = total_required_mels - active_acquire_heap->mel_count;
	if (inactive_heap_required_mels != 0) {
		ai_copy_mels_from_acquire_heap(inactive_acquire_heap, ai_in_cursor, inactive_heap_required_mels);
		ai_in_cursor += SP_MEL_NUM_BINS * inactive_heap_required_mels;
	}

	/* Then grab the remaining ones from the active acquire heap */
	ai_copy_mels_from_acquire_heap(active_acquire_heap, ai_in_cursor, active_acquire_heap->mel_count);

	/* Now normalize the data before running inference */
	ai_normalize_data(heap->ai_in_data, AI_NETWORK_IN_1_SIZE);

	return 1;
}

/*
 * Copies mel frames from an acquire heap to dst.
 * Reorders frames from order in the rotating buffer to
 * chronological order.
 */
void ai_copy_mels_from_acquire_heap(struct audio_acquire_heap_t *acquire_heap,
		ai_float* dst, uint32_t frame_count){

	/* figure out how many frames from before and after the buffer cursor are needed */
	uint32_t frames_from_begin, frames_from_end;
	if (acquire_heap->mel_cursor > frame_count){
		frames_from_begin = frame_count;
	} else {
		frames_from_begin = acquire_heap->mel_cursor;
	}
	frames_from_end = frame_count - frames_from_begin;

	/* First copy the oldest frames */
	ai_float *buffer_end = ((ai_float *) acquire_heap->mels) + SP_MEL_NUM_BINS * AUDIO_MEL_ROLING_BUFFER_SIZE;
	ai_float *buffer_tail_start = buffer_end - SP_MEL_NUM_BINS * frames_from_end;
	memcpy(dst,
			buffer_tail_start,
			sizeof(ai_float) * SP_MEL_NUM_BINS * frames_from_end);

	/* Now copy the newest frames */
	memcpy(dst + SP_MEL_NUM_BINS * frames_from_end,
			acquire_heap->mels,
			sizeof(ai_float) * SP_MEL_NUM_BINS * frames_from_begin);
}

/*
 * Normalize data in a buffer in order to prepare it for inference
 */
void ai_normalize_data(ai_float *data, uint32_t len){
	float32_t max, min;
	uint32_t tmp_index;

	arm_max_f32(data, AI_NETWORK_IN_1_SIZE, &max, &tmp_index);
	arm_min_f32(data, AI_NETWORK_IN_1_SIZE, &min, &tmp_index);

	float32_t delta = max - min;
	for(float32_t *cursor = data; cursor < data + AI_NETWORK_IN_1_SIZE; cursor++){
		*cursor = (*cursor - min) / delta;
	}
}
