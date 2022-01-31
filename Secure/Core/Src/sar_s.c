/*
 * sar_s.c
 *
 *  Created on: Feb 28, 2021
 *      Author: AUTHOR_NAME_REMOVED
 */

#include "main.h"
#include "sar_s.h"
#include "dma.h"
#include "printf.h"
#include "string.h"
#include "profiling.h"

//#define USEWATCHDOG

/****************************************************************
 * main SAR code
 ***************************************************************/

/*
 * Main entry point for the non-secure application access SAR functionality.
 * Function is NSC.
 * @param req_phase 	the SAR target phase
 * @param duration		if req_phase is SAR_PHASE_ACQUIRE or SAR_PHASE_PROCESS:
 * 							how long the function pointed to by `pc` should run for.
 * 						Otherwise ignored
 * @param pc			if req_phase is SAR_PHASE_ACQUIRE or SAR_PHASE_PROCESS:
 * 							address of the function to run in the target phase
 * 						Otherwise ignored
 * @param user_call_data	if req_phase is SAR_PHASE_ACQUIRE or SAR_PHASE_PROCESS:
 * 								pointer to be passed to the callee function via
 * 								the `sar_call_data` struct
 * @return the SAR phase active on return (either SAR_PHASE_IDLE or SAR_PHASE_FULLACCESS)
 */
uint32_t sar_entry_gateway(uint32_t req_phase, uint32_t duration, void * pc, void * user_call_data){

	PROF_WRITE(PROF_POINT_S_ENTRY_GW);

	/* stop the SAR_FULLACCESS timer. We have entered the sar gateway, so there is no more risk
	 * of an attacker extending the time spend in FULLACCESS mode */
	sar_fullaccess_timer_stop();

#ifdef USEWATCHDOG
	/* reset the watchdog. This is done to ensure that the nonsecure world must periodically call
	 * the SAR */
	watchdog_reset();
#endif

	/* Initially set exit_phase to req_phase.
	 * If req_phase is ACQUIRE or PROCESS this will be overwritten
	 * when the ACQUIRE or PROCESS phases return */
	uint32_t exit_phase = req_phase;

	/* the ACQUIRE and PROCESS phase are entered using a nonsecure callback function
	 * the return value of this function determs if on return the CPU is returned to
	 * the nonsecure application in the IDLE of FULLACCESS phase */
	if(req_phase == SAR_PHASE_ACQUIRE || req_phase == SAR_PHASE_PROCESS){

		/* check if the requested duration to stay in AQCUIRE OR PROCESS modes is acceptable */
		if(duration > (SAR_MAX_AQUIRE_PROCESS_US / SAR_CONTAINER_TIMER_PERIOD)){
			sar_error_forever_loop(SAR_ERROR_CODE_CONTAINER_REQ_TOO_LONG);
		}


		void *task_stack_pointer = NULL;
		if(req_phase == SAR_PHASE_ACQUIRE){
			task_stack_pointer = sar_prepare_for_acquire();
		} else{
			task_stack_pointer = sar_prepare_for_process();
		}

		/* Prepare the stack for the call.
		 * Call data for the callee function is placed first on top of the execution zone.
		 * The actual stack starts right underneath that. Note that we are writing to non-secure
		 * memory locations, but that is ok, as these should be non-secure accesible at this point.
		 */
		void * procedure_stack = ((char *) task_stack_pointer) - sizeof(struct sar_call_data);
		struct sar_call_data * call_data = (struct sar_call_data *) (((char *) procedure_stack) + 1);
		/* Subtracting offset to get non-secure address */
		call_data->timer_value 		= (uint32_t *) SAR_ptr_to_ns(&(SAR_CONTAINER_TIMER_TIM->CNT));
		call_data->max_time 		= duration;
		call_data->active_buffer 	= sar_active_buffer->base;
		call_data->user_call_data   = user_call_data;

		/* The timer has two purposes
		 * 1. It provides a timebase the the callee function
		 * 2. It is used to enforce that we return to the caller function after
		 * 		`duration` time, to avoid the callee function from using execution
		 * 		time as a covert channel.
		 */
		sar_container_timer_start();

		PROF_WRITE(PROF_POINT_S_GW_CALL_NS);

		/* make the callback to the callee function in the nonsecure world*/
		exit_phase = sar_nonsecure_call(call_data, pc, procedure_stack);

		PROF_WRITE(PROF_POINT_S_GW_REENTRY);

		/* Assert CRC reset signal, make sure it is clocked first */
		RCC->AHB1ENR |= RCC_AHB1ENR_CRCEN;
		RCC->AHB1RSTR |= RCC_AHB1RSTR_CRCRST;

		/* Invalidate ICACHE, to prevent data leaking through it.
		 * Done before waiting for the container timer, so it can
		 * invalidate while we are waiting anyway. */
		ICACHE->CR |= ICACHE_CR_CACHEINV;

#ifndef SAR_IGNORE_CONTAINER_TIMER

		PROF_WRITE(PROF_POINT_S_TIMER_WAIT_START);

		/* after the callee function returns, wait until `duration` time expires */
		if (sar_wait_for_container_timer(duration))
			/* If the callee executed for too long, enter infinite loop */
			sar_error_forever_loop(SAR_ERROR_CODE_CONTAINER_TIMER);

		PROF_WRITE(PROF_POINT_S_TIMER_WAIT_END);

#endif /* SAR_IGNORE_CONTAINER_TIMER */

		/* Deassert CRC reset signal */
		RCC->AHB1RSTR &= ~RCC_AHB1RSTR_CRCRST;
	}

	/* The IDLE and FULLACCESS phases are not entered using a callback, instead
	 * the MCU is placed in the correct phase and we return to the main nonsecure
	 * application. TheSAR_FULLACCESS_TIMER is used to ensure that the nonsecure world
	 * cannot stay in the FULLACCESS phase indefinitely without retrigering a
	 * notification. */
	if(exit_phase == SAR_PHASE_IDLE){
		sar_prepare_for_idle();

		PROF_WRITE(PROF_POINT_S_GW_LEAVE_IDLE);
		return SAR_PHASE_IDLE;
	} else if (exit_phase == SAR_PHASE_FULLACCESS){
		sar_prepare_for_fullaccess();
		sar_fullaccess_timer_start();
		if (sar_notification_do()) sar_error_forever_loop(SAR_ERROR_CODE_NOTIFICATION_FAILED);
		PROF_WRITE(PROF_POINT_S_GW_LEAVE_FULLACCESS);
		return SAR_PHASE_FULLACCESS;
	} else {
		sar_error_forever_loop(SAR_ERROR_CODE_WRONG_EXIT_PHASE);
	}
	sar_error_forever_loop(SAR_ERROR_CODE_GATEWAY_BOTTOM);
	return -1;
}

/*
 * Initialise the data structures and peripherals needed for SAR operation
 */
void sar_init(){
	uint32_t now = HAL_GetTick();

	/* Allow secure read/write to non-secure memory */
	GTZC_MPCBB2->CR |= GTZC_MPCBB_SRWILADIS_DISABLE;

	/* buffer 1 */
	sar_buffer1.base = SAR_BUFFER1_BASE;
	sar_buffer1.top = SAR_BUFFER1_TOP;
	sar_buffer1.top_max = SAR_BUFFER1_TOP;
	sar_buffer1.type = SAR_ZONE_TYPE_BUFFER;
	sar_buffer1.oldest_data = now;
	sar_wipe_zone(&sar_buffer1, now);

	/* buffer 2 */
	sar_buffer2.base = SAR_BUFFER2_BASE;
	sar_buffer2.top = SAR_BUFFER2_TOP;
	sar_buffer2.top_max = SAR_BUFFER2_TOP;
	sar_buffer2.type = SAR_ZONE_TYPE_BUFFER;
	sar_buffer2.oldest_data = now;
	sar_wipe_zone(&sar_buffer2, now);

	/* scratch */
	sar_scratch.base = SAR_SCRATCH_BASE;
	sar_scratch.top = SAR_SCRATCH_TOP;
	sar_scratch.top_max = SAR_SCRATCH_TOP;
	sar_scratch.type = SAR_ZONE_TYPE_SCRATCH;
	sar_scratch.oldest_data = now;
	sar_wipe_zone(&sar_scratch, now);

	/* code */
	sar_code.base = FLASH_NS_CODE_BASE;
	sar_code.top = FLASH_NS_CODE_TOP;
	sar_code.type = SAR_ZONE_TYPE_CODE;

	/* virtual microphone */
	sar_virtmic.base = SAR_VIRTMIC_BASE;
	sar_virtmic.top = SAR_VIRTMIC_TOP;
	sar_virtmic.type = SAR_ZONE_TYPE_RAM;
	sar_wipe_zone(&sar_virtmic, now);

	/* ns_ram */
	sar_nsram.base = NS_RAM_BASE;
	sar_nsram.top = NS_RAM_TOP;
	sar_nsram.type = SAR_ZONE_TYPE_RAM;

	/* timer */
	sar_timer.base = SAR_TIMER_BASE;
	sar_timer.top = SAR_TIMER_TOP;
	sar_timer.type = SAR_ZONE_TYPE_PERIPHERAL;

	/* crc */
	sar_crc.base = SAR_CRC_BASE;
	sar_crc.top = SAR_CRC_TOP;
	sar_crc.type = SAR_ZONE_TYPE_PERIPHERAL;

	/* uart */
	sar_uart.base = SAR_UART_BASE;
	sar_uart.top = SAR_UART_TOP;
	sar_uart.type = SAR_ZONE_TYPE_PERIPHERAL;

	/* gpiod */
	sar_gpiod.base = SAR_GPIOD_BASE;
	sar_gpiod.top = SAR_GPIOD_TOP;
	sar_gpiod.type = SAR_ZONE_TYPE_PERIPHERAL;

	/* gpiod */
	sar_gpioa.base = SAR_GPIOA_BASE;
	sar_gpioa.top = SAR_GPIOA_TOP;
	sar_gpioa.type = SAR_ZONE_TYPE_PERIPHERAL;


	/* set active buffer */
	sar_active_buffer = &sar_buffer1;


	sar_container_timer_init();
	sar_fullaccess_timer_init();
	sar_notification_init();

#ifdef USEWATCHDOG
	watchdog_start();
#endif

	sar_prepare_for_idle();
}

/*
 * Setup the GTZ and MPU for SAR_PHASE_IDLE
 */
void sar_prepare_for_idle(){
	/* set up GTZC */
	GTZC_TZSC->SECCFGR1 |= (1 << 4); // TIM6SEC
	GTZC_TZSC->SECCFGR1 |= (1 << 5); // TIM7SEC
	GTZC_MPCBB2->VCTR[0] = ~0x0; // Buffer1
	GTZC_MPCBB2->VCTR[1] = ~0x0;
	GTZC_MPCBB2->VCTR[2] = ~0x0; // Buffer2
	GTZC_MPCBB2->VCTR[3] = ~0x0;
	GTZC_MPCBB2->VCTR[4] = ~0x0; // Scratch
	GTZC_MPCBB2->VCTR[5] = ~0x0;
	GTZC_MPCBB2->VCTR[6] = ~0x0; // Virtual mic
	GTZC_MPCBB2->VCTR[7] = ~0x0; // Spare

	/* Set up MPU_NS */
	mpu_disable_ns();

	/* Disable FULL_ACCESS Led */
	LED_GREEN_GPIO_Port->BSRR |= LED_GREEN_Pin << 16;

}

/*
 * Setup the GTZC and MPU for SAR_PHASE_ACQUIRE
 * returns pointer to stack pointer for ACQUIRE task
 */
void *sar_prepare_for_acquire(){

	dma2_disable();

	/* Set up GTZC */
	GTZC_TZSC->SECCFGR1 |= (1 << 4); // TIM6SEC
	GTZC_TZSC->SECCFGR1 &= ~(1 << 5); // TIM7SEC
	GTZC_MPCBB2->VCTR[0] = 0x0; // Buffer1
	GTZC_MPCBB2->VCTR[1] = 0x0;
	GTZC_MPCBB2->VCTR[2] = 0x0; // Buffer2
	GTZC_MPCBB2->VCTR[3] = 0x0;
	GTZC_MPCBB2->VCTR[4] = ~0x0; // Scratch
	GTZC_MPCBB2->VCTR[5] = ~0x0;
	GTZC_MPCBB2->VCTR[6] = 0x0; // Virtual mic
	GTZC_MPCBB2->VCTR[7] = ~0x0; // Spare

	/* Select target buffer and to clearing of buffers if needed */
	sar_maintain_buffers(0);

	/* Set up MPU_NS */
	mpu_clear_ns();

	uint32_t region_number = 1;
	mpu_grant_access(&sar_virtmic, SAR_ACCESS_LEVEL_RO, region_number++);
	mpu_grant_access(sar_active_buffer, SAR_ACCESS_LEVEL_RW, region_number++);
	mpu_grant_access(&sar_code, SAR_ACCESS_LEVEL_RO, region_number++);
	mpu_grant_access(&sar_nsram, SAR_ACCESS_LEVEL_RO, region_number++);
	mpu_grant_access(&sar_timer, SAR_ACCESS_LEVEL_RO, region_number++);
#if SAR_DEBUG_UART == 1
	mpu_grant_access(&sar_uart, SAR_ACCESS_LEVEL_RW, region_number++);
#endif
#ifdef PROFILE
	mpu_grant_access(&sar_gpiod, SAR_ACCESS_LEVEL_RW, region_number++);
#endif
#ifdef MACRO_EVALUATION
	mpu_grant_access(&sar_gpioa, SAR_ACCESS_LEVEL_RW, region_number++);
#endif

#ifndef SAR_NO_MPU
	mpu_enable_ns();
#else
	mpu_disable_ns();
#endif /* SAR_NO_MPU */



	/* Disable FULL_ACCESS Led */
	LED_GREEN_GPIO_Port->BSRR |= LED_GREEN_Pin << 16;

	return sar_active_buffer->top;
}

/*
 * Setup the GTZC and MPU for SAR_PHASE_PROCESS
 * returns pointer to stack pointer for PROCESS task
 */
void *sar_prepare_for_process(){

	dma2_disable();

	/* Set up GTZC */
	GTZC_TZSC->SECCFGR1 |= (1 << 4); // TIM6SEC
	GTZC_TZSC->SECCFGR1 &= ~(1 << 5); // TIM7SEC
	GTZC_MPCBB2->VCTR[0] = 0x0; // Buffer1
	GTZC_MPCBB2->VCTR[1] = 0x0;
	GTZC_MPCBB2->VCTR[2] = 0x0; // Buffer2
	GTZC_MPCBB2->VCTR[3] = 0x0;
	GTZC_MPCBB2->VCTR[4] = 0x0; // Scratch
	GTZC_MPCBB2->VCTR[5] = 0x0;
	GTZC_MPCBB2->VCTR[6] = ~0x0; // Virtual mic
	GTZC_MPCBB2->VCTR[7] = ~0x0; // Spare

	/* Select target buffer and do clearing of buffers if needed */
	sar_maintain_buffers(0);

	/* Set up MPU_NS */
	mpu_clear_ns();

	uint32_t region_number = 1;
	mpu_grant_access(&sar_buffer1, SAR_ACCESS_LEVEL_RO, region_number++);
	mpu_grant_access(&sar_buffer2, SAR_ACCESS_LEVEL_RO, region_number++);
	mpu_grant_access(&sar_scratch, SAR_ACCESS_LEVEL_RW, region_number++);
	mpu_grant_access(&sar_code, SAR_ACCESS_LEVEL_RO, region_number++);
	mpu_grant_access(&sar_nsram, SAR_ACCESS_LEVEL_RO, region_number++);
	mpu_grant_access(&sar_timer, SAR_ACCESS_LEVEL_RO, region_number++);
	mpu_grant_access(&sar_crc, SAR_ACCESS_LEVEL_RW, region_number++);
#if SAR_DEBUG_UART == 1
	mpu_grant_access(&sar_uart, SAR_ACCESS_LEVEL_RW, region_number++);
#endif
#ifdef PROFILE
	mpu_grant_access(&sar_gpiod, SAR_ACCESS_LEVEL_RW, region_number++);
#endif
#ifdef MACRO_EVALUATION
	mpu_grant_access(&sar_gpioa, SAR_ACCESS_LEVEL_RW, region_number++);
#endif

#ifndef SAR_NO_MPU
	mpu_enable_ns();
#else
	mpu_disable_ns();
#endif /* SAR_NO_MPU */
	/* Disable FULL_ACCESS Led */
	LED_GREEN_GPIO_Port->BSRR |= LED_GREEN_Pin << 16;

	return sar_scratch.top;
}

/*
 * Setup the GTZ and MPU for SAR_PHASE_FULLACCESS
 */
void sar_prepare_for_fullaccess(){
	/* Set up GTZC */
	GTZC_TZSC->SECCFGR1 |= (1 << 4); // TIM6SEC
	GTZC_TZSC->SECCFGR1 |= (1 << 5); // TIM7SEC
	GTZC_MPCBB2->VCTR[0] = 0x0; // Buffer1
	GTZC_MPCBB2->VCTR[1] = 0x0;
	GTZC_MPCBB2->VCTR[2] = 0x0; // Buffer2
	GTZC_MPCBB2->VCTR[3] = 0x0;
	GTZC_MPCBB2->VCTR[4] = 0x0; // Scratch
	GTZC_MPCBB2->VCTR[5] = 0x0;
	GTZC_MPCBB2->VCTR[6] = 0x0; // Virtual mic
	GTZC_MPCBB2->VCTR[7] = ~0x0; // Spare

	/* Select target buffer and do clearing of buffers if needed */
	sar_maintain_buffers(0);

	/* Set up MPU_NS */
	mpu_disable_ns();

	/* Enable FULL_ACCESS Led */
	LED_GREEN_GPIO_Port->BSRR |= LED_GREEN_Pin;
}

/*
 * Calling this function wipes (zeros) all three of the SAR zones.
 * Function is NSC.
 */
uint32_t sar_wipe_all_zones(void){

	uint32_t now = HAL_GetTick();

	sar_wipe_zone(&sar_buffer1, now);
	sar_wipe_zone(&sar_buffer2, now);
	sar_wipe_zone(&sar_scratch, now);

	return 0;
}

/*
 * Assembly voodoo used to make call the callee functions that run in
 * AQCUIRE and PROCESS. Ensures that no data can leak out of these
 * functions through CPU registers.
 *
 * @param call_data		pointer to be passed to the callee
 * @param pc			function to call
 * @param sp			stack for the callee
 */
uint32_t sar_nonsecure_call(void * call_data, void * pc , void * sp){
	  asm (
		"PUSH	{lr} \n\t"
// 1. clear unused caller-saved registers
		"MOV	r3, r0 \n\t"

// 2. push callee-saved registers to the stack
		"PUSH 	{r4-r12} \n\t"

// 3. Push special and banked registers to the stack
		"MRS	r4, MSP_NS \n\t"
		"MRS	r5, PSP_NS \n\t"
		"MRS	r6, PRIMASK_NS \n\t"
		"MRS	r7, FAULTMASK_NS \n\t"
		"MRS	r8, BASEPRI_NS \n\t"
		"MRS    r9, CONTROL_NS \n\t"
		"MRS    r10, PSPLIM_NS \n\t"
		"MRS    r11, MSPLIM_NS \n\t"
		"PUSH 	{r4-r11} \n\t"

// 4. Push FPSCR and clear it.
		"VMRS	r4, FPSCR \n\t"
		"PUSH   {r4} \n\t"
		"MOV	r4, #0 \n\t"
		"VMSR   FPSCR, r4 \n\t"

// 5. Clear callee-saved registers
		"MOV	r4, r0 \n\t"
		"MOV	r5, r0 \n\t"
		"MOV	r6, r0 \n\t"
		"MOV	r7, r0 \n\t"
		"MOV	r8, r0 \n\t"
		"MOV	r9, r0 \n\t"
		"MOV	r10, r0 \n\t"
		"MOV	r11, r0 \n\t"
		"MOV	r12, r0 \n\t"

// 6. clear caller-saved floating point registers
		"VMOV	s0, s1, r0, r0 \n\t"
		"VMOV	s2, s3, r0, r0 \n\t"
		"VMOV	s4, s5, r0, r0 \n\t"
		"VMOV	s6, s7, r0, r0 \n\t"
		"VMOV	s8, s9, r0, r0 \n\t"
		"VMOV	s10, s11, r0, r0 \n\t"
		"VMOV	s12, s13, r0, r0 \n\t"
		"VMOV	s14, s15, r0, r0 \n\t"

// 7. push callee-saved floating point registers to the stack and clear them
		"VPUSH	{s16-s31} \n\t"
		"VMOV	s16, s17, r0, r0 \n\t"
		"VMOV	s18, s19, r0, r0 \n\t"
		"VMOV	s20, s21, r0, r0 \n\t"
		"VMOV	s22, s23, r0, r0 \n\t"
		"VMOV	s24, s25, r0, r0 \n\t"
		"VMOV	s26, s27, r0, r0 \n\t"
		"VMOV	s28, s29, r0, r0 \n\t"
		"VMOV	s30, s31, r0, r0 \n\t"

// A. clear APSR, other CPSR bits are RO
		"MSR   	APSR_nzcvqg, r0 \n\t"

// set non-secure stack poinger
		"MSR    MSP_NS, r2 \n\t"

// clear LSB and make call to actual function
		"LSRS	r1, r1, #1 \n\t"
		"LSLS   r1, r1, #1 \n\t"
		"BLXNS	r1 \n\t"

// 7. restore callee-saved floating point registers
		"VPOP	{s16-s31} \n\t"

// 6. clear caller-saved floating point registers
		"MOV	r4, #0 \n\t"
		"VMOV	s0, s1, r4, r4 \n\t"
		"VMOV	s2, s3, r4, r4 \n\t"
		"VMOV	s4, s5, r4, r4 \n\t"
		"VMOV	s6, s7, r4, r4 \n\t"
		"VMOV	s8, s9, r4, r4 \n\t"
		"VMOV	s10, s11, r4, r4 \n\t"
		"VMOV	s12, s13, r4, r4 \n\t"
		"VMOV	s14, s15, r4, r4 \n\t"

// 5. no need to clear callee-saved FPRs at this point

// 4. Restore FPSCR
		"POP   	{r4} \n\t"
		"VMSR   FPSCR, r4 \n\t"

// 3. restore special registers
		"POP 	{r4-r11} \n\t"
		"MSR	MSP_NS, r4 \n\t"
		"MSR	PSP_NS, r5 \n\t"
		"MSR	PRIMASK_NS, r6 \n\t"
		"MSR	FAULTMASK_NS, r7 \n\t"
		"MSR	BASEPRI_NS, r8 \n\t"
		"MSR    CONTROL_NS, r9 \n\t"
		"MSR    PSPLIM_NS, r10 \n\t"
		"MSR    MSPLIM_NS, r11 \n\t"

// 2. restore callee-saved registers
		"POP 	{r4-r12} \n\t"

// 1. clear caller-saved registers
// Note that r0 is _not_ cleared on purpose!
// R0 is used to let function choose to enter FULL_ACCESS
		"MOV	r1, #0 \n\t"
		"MOV	r2, #0 \n\t"
		"MOV	r3, #0 \n\t"

// A. clear APSR, other CPSR bits are RO
		"MSR   	APSR_nzcvqg, r1 \n\t"

// restore LR, and return to previous world
		"POP	{lr} \n\t"
		"BX		lr \n\t"
    );
}

/****************************************************************
 * sar_zones and buffers
 ***************************************************************/

/*
 * Wrapper for sar_maintain_buffers
 * Function is NSC
 */
uint32_t sar_app_maintain_buffers(uint32_t margin){
	return sar_maintain_buffers(margin);
}

/*
 * Check if buffers need to be rotated and/or wiped
 * within margin ms from now, and if so, rotate and
 * wipe the buffers.
 *
 * Returns 1 if buffers have been wiped
 */
uint32_t sar_maintain_buffers(uint32_t margin){
	uint32_t now = HAL_GetTick();
	uint32_t wiped = 0;

	struct sar_zone_t *inactive_buffer;
	if (sar_active_buffer == &sar_buffer1){
		inactive_buffer = &sar_buffer2;
	} else if (sar_active_buffer == &sar_buffer2) {
		inactive_buffer = &sar_buffer1;
	} else {
		sar_error_forever_loop(SAR_ERROR_CODE_BUFFER);
	}

	/*  IF either
	 *  	1. the active buffer is older than SAR_BUFFER_ROTATION_MS, or
	 *  	2. the inactive buffer is older than 2xSAR_BUFFER_ROTATION_MS,
	 */
	uint32_t swap_needed =
			(sar_active_buffer->oldest_data + SAR_BUFFER_ROTATION_MS < now + margin)
		||  (inactive_buffer->oldest_data + 2 * SAR_BUFFER_ROTATION_MS < now + margin);

	/*
	 *  THEN:
	 *  	1. Wipe the inactive buffer, and
	 *  	2. Swap the buffers
	 *  	3. Check the age of the new inactive buffer. When it is too old,
	 *  	   wipe it too.
	 */

	if(swap_needed){
		/* 1. wipe inactive */
		wiped = 1;
		sar_wipe_zone(inactive_buffer, now);

		/* 2. swap buffers */
		struct sar_zone_t *tmp_buffer = sar_active_buffer;
		sar_active_buffer = inactive_buffer;
		inactive_buffer = tmp_buffer;

		/* 3. check new inactive, wipe as needed */
		if (inactive_buffer->oldest_data + 2 * SAR_BUFFER_ROTATION_MS < now + margin){
			wiped = 1;
			sar_wipe_zone(inactive_buffer, now);
		}
	}

	/* IF any buffer got wiped
	 * THEN also wipe the scratch zone
	 */
	if (wiped){
		sar_wipe_zone(&sar_scratch, now);
	}

	return wiped;
}

/*
 * Wipe memory from start to end
 */
void sar_wipe_memory_region(uint32_t *start, uint32_t* end){
	MACRO_SET_WIPING();
	PROF_WRITE(PROF_POINT_S_WIPE_START);
	/* Not using memset, as that was slower */
	for (uint32_t *cursor = start; cursor <  end; cursor++){
			*cursor = 0;
	}
	MACRO_RESET_WIPING();
	PROF_WRITE(PROF_POINT_S_WIPE_END);
}

/*
 * Wipe a SAR zone
 * @param zone	the zone to be wiped
 * @param time	the current time
 */
void sar_wipe_zone(struct sar_zone_t * zone, uint32_t time){
	//printf_("sar_wipe_zone() 0x%p, %u\n", zone->base, time);
	sar_wipe_memory_region(
			SAR_ptr_to_s(zone->base),
			SAR_ptr_to_s(zone->top));
	zone->oldest_data = time;
}

/****************************************************************
 * SAR_FULLACCESS_TIMER
 ***************************************************************/

/*
 * Configure SAR_FULLACCESS_TIMER as a one pulse timer.
 * This timer is used provide timing information to callee
 * functions running in ACQUIRE or PROCESS
 */
void sar_fullaccess_timer_init(void){
	SAR_FULLACCESS_TIMER->CR1 		= (0 << 11) // UIFREMAP
									| (0 << 7)  // ARPE
									| (1 << 3)  // OPM
									| (1 << 2)  // URS
									| (0 << 1)  // UDIS
									| (0 << 0)  // CEN
									;
	SAR_FULLACCESS_TIMER->CR2 		= 0;
	SAR_FULLACCESS_TIMER->DIER 		= (0 << 8) // UDE
									| (1 << 0); // UIE

	SAR_FULLACCESS_TIMER->PSC 		= SAR_FULLACCESS_TIMER_PRESCALER - 1;
	SAR_FULLACCESS_TIMER->ARR		= SAR_FULLACCESS_TIMER_ARR;
	SAR_FULLACCESS_TIMER->EGR	    |= (1 << 0);  // UG
	SAR_FULLACCESS_TIMER->SR		&= ~(1 << 0); // UIF

	DBGMCU->APB1FZR1           		|= (1 << 4);  // DBG_TIM7_STOP

	// Enable TIM6 interrupts
	uint32_t prioritygroup = NVIC_GetPriorityGrouping();
	NVIC_SetPriority(TIM6_IRQn, NVIC_EncodePriority(prioritygroup, 0, 0));
	NVIC_ClearPendingIRQ(TIM6_IRQn);
	NVIC_EnableIRQ(TIM6_IRQn);
}

/*
 * Reset and start the SAR_CONTAINER_TIMER
 */
void sar_fullaccess_timer_start(){
	/* generate update to reset and stop timer */
	SAR_FULLACCESS_TIMER->EGR	    |= (1 << 0);  // UG
	/* clear interrupt flag */
	SAR_FULLACCESS_TIMER->SR		&= ~(1 << 0); // UIF
	/* Start timer */
	SAR_FULLACCESS_TIMER->CR1      	|= (1 << 0);  // CEN
}

/*
 * Stop the SAR_CONTAINER_TIMER
 */
void sar_fullaccess_timer_stop(){
	SAR_FULLACCESS_TIMER->CR1      	&= ~(1 << 0);  // CEN
}

/*
 * Calculate how much time is left in full access mode.
 * Returns remaining time in us.
 * Function is NSC
 */
uint32_t sar_fullaccess_time_remaining(void){
	/* if timer is enabled then we compute time left.
	 * The plus one is needed because the counter only overflows at value ARR + 1 */
	if(SAR_FULLACCESS_TIMER->CR1 & 0b1){
		uint32_t remaining_time = (0xFFFF & SAR_FULLACCESS_TIMER->ARR) - (0xFFFF & SAR_FULLACCESS_TIMER->CNT);
		// TODO: there might be an off-by-one error here.
		remaining_time = (remaining_time + 1) * SAR_FULLACCESS_TIMER_PERIOD;
		return remaining_time;
	} else {
		return 0;
	}
}

/*
 * This interrupt fires when the end of FULLACCESS mode is reached.
 * One _could_ just simply move to IDLE mode here, but that introduces
 * a set of annoying timing problems, e.g., when this interrupt would fire
 * while executing a call to the sar_entry_gateway.
 *
 * Instead, we just assume that this interrupt should _never_ fire, and
 * turn it in to an infinite loop.
 */
void TIM6_IRQHandler(void) {
	sar_error_forever_loop(SAR_ERROR_CODE_FULLACCESS_TIMER);
}


/****************************************************************
 * SAR_CONTAINER_TIMER
 ***************************************************************/

/*
 * Configure SAR_CONTAINER_TIMER as a one pulse timer.
 * This timer is used provide timing information to callee
 * functions running in ACQUIRE or PROCESS
 */
void sar_container_timer_init(void){
	SAR_CONTAINER_TIMER_TIM->CR1 		= (1 << 11) // UIFREMAP
										| (0 << 7)  // ARPE
										| (1 << 3)  // OPM
										| (1 << 2)  // URS
										| (0 << 1)  // UDIS
										| (0 << 0)  // CEN
										;
	SAR_CONTAINER_TIMER_TIM->CR2 		= 0;
	SAR_CONTAINER_TIMER_TIM->DIER 		= (0 << 8) // UDE
										| (1 << 0); // UIE

	SAR_CONTAINER_TIMER_TIM->PSC 		= SAR_CONTAINER_TIMER_PRESCALER - 1;

	SAR_CONTAINER_TIMER_TIM->EGR	    |= (1 << 0);  // UG

	DBGMCU->APB1FZR1           			|= (1 << 5);  // DBG_TIM7_STOP
}

/*
 * Reset and start the SAR_CONTAINER_TIMER
 */
void sar_container_timer_start(){
	/* generate update to reset and stop timer */
	SAR_CONTAINER_TIMER_TIM->EGR	    |= (1 << 0);  // UG
	/* clear interupt flag */
	SAR_CONTAINER_TIMER_TIM->SR			&= ~(1 << 0); // UIF
	/* Start timer */
	SAR_CONTAINER_TIMER_TIM->CR1      	|= (1 << 0);  // CEN
}

/*
 * Stop the SAR_CONTAINER_TIMER
 */
void sar_container_timer_stop(){
	SAR_CONTAINER_TIMER_TIM->CR1      	&= ~(1 << 0);  // CEN
}

/*
 * Wait for the SAR_CONTAINER_TIMER to reach the target value and return 0
 * Returns 1 if timer already reached the target value when entering the call
 */
uint32_t sar_wait_for_container_timer(uint32_t target){
	if (SAR_CONTAINER_TIMER_TIM->CNT >= target){
		return 1;
	}
	//printf_("CONTAINER TIME: %u\n", SAR_CONTAINER_TIMER_TIM->CNT);
	while (SAR_CONTAINER_TIMER_TIM->CNT < target);
	return 0;
}


/****************************************************************
 * sar_notification
 ***************************************************************/
/*
 * Init the timer used for sending the SAR notification.
 * I.e., for blinking LED_BLUE
 */
void sar_notification_init(void){
	TIM4->CR1 	= (0b0 << 11) 		// UIFREMAP
				| (0b00 << 8) 		// CKD
				| (0b0 << 7)		// ARPE
				| (0b00 << 5)		// CMS
				| (0b0 << 4)		// DIR
				| (0b1 << 3)		// OPM
				| (0b0 << 2)		// URS
				| (0b0 << 1)		// UDIS
				| (0b0 << 0)		// CEN
				;

	TIM4->CR2	= (0b0 << 7)		// TI1S
				| (0b000 << 4)		// MMS
				| (0b0 << 3)		// CCDS
				;

	TIM4->SMCR	= (0b0 << 15)		// ETP
				| (0b0 << 14)		// ECE
				| (0b00 << 12)		// ETPS
				| (0b0000 << 8)		// ETF
				| (0b0 << 7)		// MSM
				| (0b111 << 4)		// TS
				| (0b0 << 16)		// SMS[3]
				| (0b000 << 0)		// SMS[2:0]
				;

	TIM4->CCMR1 = (0b0 << 15)		// OC2CE
				| (0b0 << 24)		// OC2M[3]
				| (0b111 << 12)		// OC2M[2:0]
				| (0b0 << 11)		// OC2PE
				| (0b0 << 10)		// OC1FE
				| (0b00 << 8)		// CC1S
				;

	TIM4->CCER = (1 << 4); // CC2E

	TIM4->PSC = 55000;
	TIM4->ARR = 1000;			// 500 ms pulse.
	TIM4->CCR2 = 1;
	TIM4->EGR |= (0b1 << 0); // UG
}

/*
 * Send the SAR notification.
 * I.e., blink LED_BLUE
 */
uint32_t sar_notification_do(void){
	TIM4->EGR |= (0b1 << 0); // UG
	TIM4->CR1 |= 0x1; // CEN
	return 0;
}

/****************************************************************
 * error signaling
 ***************************************************************/

/*
 * Count down from count to 0
 */
void sar_error_delay(uint32_t count){
	while (count--) asm("");
}

/*
 * Loop forever while blinking out an error code
 */
void sar_error_forever_loop(uint32_t error_code){
	while(1){
		SAR_ERROR_GPIO_Port->BSRR = SAR_ERROR_PIN << 16;
		sar_error_delay(SAR_ERROR_SPACE_TIME);
		for (uint32_t i = 0; i < error_code; i++){
			SAR_ERROR_GPIO_Port->BSRR = SAR_ERROR_PIN;
			sar_error_delay(SAR_ERROR_ON_TIME);
			SAR_ERROR_GPIO_Port->BSRR = SAR_ERROR_PIN << 16;
			sar_error_delay(SAR_ERROR_OFF_TIME);
		}
	}
}


/****************************************************************
 * MPU
 ***************************************************************/

/*
 * Enabled MPU_NS
 * This function only work when executed in the secure world.
 * Otherwise the alias registers are not available, and the MPU_NS
 * must be accessed using its normal registers.
 */
void mpu_enable_ns(void){
	*MPU_CTRL_NS = 0b011; // !PRIVDEFENA | HFNMIENA | ENABLE
	__DSB();
	__ISB();
}

/*
 * Disable MPU_NS
 */
void mpu_disable_ns(void){
	*MPU_CTRL_NS &= ~0x1; // !PRIVDEFENA | HFNMIENA | !ENABLE
	__DSB();
	__ISB();
}

/*
 * Disables all MPU_NS regions and sets attribute indirection register as expected
 */
void mpu_clear_ns(void){
	for (uint8_t mpu_region = 1; mpu_region <= MPU_REGION_COUNT; mpu_region++){
		*MPU_RNR_NS = mpu_region;
		*MPU_RLAR_NS &= ~0x1;
	}

	*MPU_MAIR0_NS = 0x0
		  | 0b10101010 << 0   // Attribute 0 (RAM)
		  | 0b10101010 << 8   // Att ribute 1 (Internal SRAM)
		  | 0b11111111 << 16  // Attribute 2 (External SRAM)
		  | 0b00000000 << 24  // Attribute 3 (Peripherals)
		  ;
}

/*
 * grant access to a SAR zone
 * @param zone	the zone to grant access to
 * @param access_level	either SAR_ACCESS_LEVEL_RO or SAR_ACCESS_LEVEL_RW
 * @param mpu_region_nr	the MPU region to configure this access in
 *
 */
void mpu_grant_access(struct sar_zone_t * zone, uint32_t access_level, uint32_t mpu_region_nr){
	/* Configure the correct region of the MPU */
	if (mpu_region_nr > MPU_REGION_COUNT){
		sar_error_forever_loop(SAR_ERROR_CODE_MPU_FULL);
	}
	*MPU_RNR_NS = mpu_region_nr;

	/* Set base address */
	*MPU_RBAR_NS = (uint32_t) zone->base & 0xffffffe0;

	/* set shareability */
	*MPU_RBAR_NS |= (0b00 << 3); // Not shareable

	/* set execution permission*/
	switch (zone->type){
	case SAR_ZONE_TYPE_BUFFER:
	case SAR_ZONE_TYPE_SCRATCH:
	case SAR_ZONE_TYPE_PERIPHERAL:
	case SAR_ZONE_TYPE_RAM:
		*MPU_RBAR_NS |= (0b1 << 0); // no execution permitted
		break;
	case SAR_ZONE_TYPE_CODE:
		*MPU_RBAR_NS |= (0b0 << 0); // execution permitted
		break;
	default:
		sar_error_forever_loop(SAR_ERROR_CODE_MPU_1);
	}

	/* set access level */
	switch (access_level){
	case SAR_ACCESS_LEVEL_RO:
		*MPU_RBAR_NS |= (0b11 << 1); // Read only by all
		break;
	case SAR_ACCESS_LEVEL_RW:
		*MPU_RBAR_NS |= (0b01 << 1); // Read-Write by all
		break;
	default:
		sar_error_forever_loop(SAR_ERROR_CODE_MPU_2);
	}

	/* set top address */
	*MPU_RLAR_NS = (uint32_t) zone->top & 0xffffffe0;

	/* set MAIR attribute */
	switch (zone->type){
	case SAR_ZONE_TYPE_BUFFER:
	case SAR_ZONE_TYPE_SCRATCH:
	case SAR_ZONE_TYPE_RAM:
		*MPU_RLAR_NS |= (0b0001 << 1); // internal sram, attribute 1;
		break;
	case SAR_ZONE_TYPE_CODE:
		*MPU_RLAR_NS |= (0b0000 << 1); // flash, attribute 0;
		break;
	case SAR_ZONE_TYPE_PERIPHERAL:
		*MPU_RLAR_NS |= (0b0011 << 1); // flash, attribute 3;
		break;
	default:
		sar_error_forever_loop(SAR_ERROR_CODE_MPU_3);
	}

	*MPU_RLAR_NS |= (0b1 << 0); //enable region
}

/****************************************************************
 * Watchdog
 ***************************************************************/

/*
 * Start the IWDG. Once started it cannot be stoped.
 */
void watchdog_start(void){
	GTZC_TZSC->SECCFGR1 |= (1 << 7); // IWDGSEC

	IWDG->KR = 0xCCCC;
	IWDG->KR = 0x5555;
	IWDG->PR = 0b110;  // prescale by 256, resulting frequency: 125 Hz
	IWDG->RLR = 638;   // 5.1 second (5 second + grace time)
	while(IWDG->SR);
	IWDG->KR = 0xAAAA;
}

/*
 * Reset the IWDG
 */
void watchdog_reset(void){
	IWDG->KR = 0xAAAA;
}

