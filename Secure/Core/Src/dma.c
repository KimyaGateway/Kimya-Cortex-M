/*
 * dma.c
 *
 *  Created on: Oct 12, 2021
 *      Author: AUTHOR_NAME_REMOVED
 */

#include "main.h"
#include "dma.h"

/*
 * Enable DMA1
 * Set alle channels to secure and disabled
 */
void dma1_init(void){
    // Enable clock to DMA and DMAMUX
	__HAL_RCC_DMA1_CLK_ENABLE();
	__HAL_RCC_DMAMUX1_CLK_ENABLE();

	// Set all DMA_A channels as secure
	DMA1_Channel1->CCR |= 0b1 << DMA_CCR_SECM_Pos;
	DMA1_Channel2->CCR |= 0b1 << DMA_CCR_SECM_Pos;
	DMA1_Channel3->CCR |= 0b1 << DMA_CCR_SECM_Pos;
	DMA1_Channel4->CCR |= 0b1 << DMA_CCR_SECM_Pos;
	DMA1_Channel5->CCR |= 0b1 << DMA_CCR_SECM_Pos;
	DMA1_Channel6->CCR |= 0b1 << DMA_CCR_SECM_Pos;
	DMA1_Channel7->CCR |= 0b1 << DMA_CCR_SECM_Pos;
	DMA1_Channel8->CCR |= 0b1 << DMA_CCR_SECM_Pos;

	// Disable all DMA_A channels
	DMA1_Channel1->CCR &= ~DMA_CCR_EN;
	DMA1_Channel2->CCR &= ~DMA_CCR_EN;
	DMA1_Channel3->CCR &= ~DMA_CCR_EN;
	DMA1_Channel4->CCR &= ~DMA_CCR_EN;
	DMA1_Channel5->CCR &= ~DMA_CCR_EN;
	DMA1_Channel6->CCR &= ~DMA_CCR_EN;
	DMA1_Channel7->CCR &= ~DMA_CCR_EN;
	DMA1_Channel8->CCR &= ~DMA_CCR_EN;
}

/*
 * Disable (clock too) dma2
 */
void dma2_disable(void){
    // Disable clock to DMA2
	__HAL_RCC_DMA2_CLK_DISABLE();
}
