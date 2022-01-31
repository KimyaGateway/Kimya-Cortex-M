/*
 * ai.h
 *
 *  Created on: Oct 8, 2021
 *      Author: AUTHOR_NAME_REMOVED
 */

#ifndef INC_AI_H_
#define INC_AI_H_

#include "network.h"
#include "network_data.h"
#include "arm_math.h"
#include "network_img_rel.h"
#include "ai_reloc_network.h"
#include "audio.h"

#define AI_INPUT_MEL_FRAMES AUDIO_MEL_ROLING_BUFFER_SIZE

struct ai_heap_t {
	/* Global handle to reference an instantiated C-model */
	ai_handle network;
	/* Heap space to put network variables */
	AI_ALIGNED(32) uint8_t rt_ai_ram[AI_NETWORK_RELOC_RAM_SIZE_XIP];
	/* Data buffer for input tensor */
	AI_ALIGNED(32) ai_float ai_in_data[AI_NETWORK_IN_1_SIZE];
	/* Data buffer for the output tensor */
	AI_ALIGNED(32) ai_float ai_out_data[AI_NETWORK_OUT_1_SIZE];
	/* Data buffer for the internal activation tensors */
	AI_ALIGNED(32) ai_u8 activations[AI_NETWORK_RELOC_ACTIVATIONS_SIZE];
};

void ai_init(struct ai_heap_t *nw_data);
void ai_run(const void *in_data, void *out_data, struct ai_heap_t *nw_data);
uint32_t ai_prepare_mels(struct audio_acquire_heap_t *active_buffer,
		struct audio_acquire_heap_t *inactive_buffer, struct ai_heap_t *heap);
void ai_copy_mels_from_acquire_heap(struct audio_acquire_heap_t *buffer,
		ai_float* dst, uint32_t count);
void ai_normalize_data(ai_float *data, uint32_t len);

#endif /* INC_AI_H_ */
