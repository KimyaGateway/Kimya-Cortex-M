/*
 * mic.c
 *
 *  Created on: Oct 9, 2021
 *      Author: AUTHOR_NAME_REMOVED
 */

#include "mic.h"
#include "sar_s.h"
#include "main.h"

/*
 * Select a memory location in which to store the alternating microphone buffers
 */
void mic_set_buffer(struct mic_memory_zone_t *location){
	mic_buffer = SAR_ptr_to_s(location);
	mic_buffer->new_data = 0;
	mic_buffer->active_buffer = SAR_ptr_to_ns(mic_buffer->buffer_a);
}

/*
 * Start the SAI and setup DMA stream from SAI
 * to alternating buffers.
 */
void mic_start_stream(SAI_HandleTypeDef *sai){
	// Enable SAI DMA Requests
	sai->Instance->CR1 |= SAI_xCR1_DMAEN;

	// Configure DMA channel
	DMA1_Channel1->CPAR = (uint32_t) &(SAI1_Block_A->DR);
	DMA1_Channel1->CM0AR = (uint32_t) mic_buffer->buffer_a;
	DMA1_Channel1->CM1AR = (uint32_t) mic_buffer->buffer_b;
	DMA1_Channel1->CNDTR = MIC_BUFFER_LEN;

	DMA1_Channel1->CCR =
			(0b1  << DMA_CCR_DSEC_Pos) |
			(0b1  << DMA_CCR_SSEC_Pos) |
			(0b1  << DMA_CCR_SECM_Pos) |
			(0b0  << DMA_CCR_CT_Pos) |
			(0b1  << DMA_CCR_DBM_Pos) |
			(0b11 << DMA_CCR_PL_Pos) |
			(0b01 << DMA_CCR_MSIZE_Pos) |
			(0b01 << DMA_CCR_PSIZE_Pos) |
			(0b1  << DMA_CCR_MINC_Pos) |
			(0b0  << DMA_CCR_PINC_Pos) |
			(0b1  << DMA_CCR_CIRC_Pos) |
			(0b0  << DMA_CCR_DIR_Pos) |
			(0b1  << DMA_CCR_TCIE_Pos)
	;

	DMAMUX1_Channel0->CCR |= 37; // Assign to SAI1

	DMA1_Channel1->CCR |= DMA_CCR_EN;
	sai->Instance->CR1 |= SAI_xCR1_SAIEN;

	// Enable DMA1_channel1 interrupts
	uint32_t prioritygroup = NVIC_GetPriorityGrouping();
	NVIC_SetPriority(DMA1_Channel1_IRQn, NVIC_EncodePriority(prioritygroup, 1, 0));
	NVIC_ClearPendingIRQ(DMA1_Channel1_IRQn);
	NVIC_EnableIRQ(DMA1_Channel1_IRQn);
}

/*
 * This interrupt fires whenever a new block of audio data is available for processing
 */
void DMA1_Channel1_IRQHandler(void) {
	DMA1->IFCR = DMA_IFCR_CTCIF1;

	// if the CT flag is set, CM1AR is used by the DMA controller,
	// so CM0AR is available for the application
	if (DMA1_Channel1->CCR & DMA_CCR_CT){
		mic_buffer->active_buffer = SAR_ptr_to_ns(mic_buffer->buffer_a);
	} else {
		mic_buffer->active_buffer = SAR_ptr_to_ns(mic_buffer->buffer_b);
	}

	mic_buffer->new_data = 1;
}

/*
 * Function intended for use by non-secure world to poll and see if there is new microphone data available yet
 * Function is NSC
 */
uint32_t mic_check_for_new_data(){
	uint32_t result;
	__set_PRIMASK(0x1);
	if (mic_buffer->new_data){
		result = 1;
	}
	else{
		result = 0;
	}
	mic_buffer->new_data = 0;
	__set_PRIMASK(0x0);
	return result;
}
