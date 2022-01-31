/*
 * sar_ns.h
 *
 *  Created on: Feb 28, 2021
 *      Author: AUTHOR_NAME_REMOVED
 */

#ifndef INC_SAR_NS_H_
#define INC_SAR_NS_H_

/* Location of the two sensor data buffers */
static uint32_t * const SAR_BUFFER1_BASE 	= (void *) 0x20030000;
static uint32_t * const SAR_BUFFER2_BASE 	= (void *) 0x20034000;
static uint32_t * const SAR_SCRATCH_BASE    = (void *) 0x20038000;

/* SAR phase definitions */
#define SAR_PHASE_IDLE 1 		/* no sensor access */
#define SAR_PHASE_ACQUIRE 2		/* isolated sensor access */
#define SAR_PHASE_PROCESS 3		/* isolated buffer access */
#define SAR_PHASE_FULLACCESS 4  /* full access to MCU */

/*
 * A demo struct used to pass data to callee functions in SAR_PHASE_ACQUIRE or
 * SAR_PHASE_PROCESS
 */
struct sar_user_call_data {
	uint32_t	test_val;
};

struct sar_call_data {
	/* Pointer to current timer value */
	uint32_t *  					timer_value;
	/* Function in ACQUIRE or PROCESS must return before
	 * *timer_value reaches max_time */
	uint32_t						max_time;
	/* Points to the currently active sensor buffer */
	uint32_t *  					active_buffer;
	/* Used by the caller (in IDLE) to pass data to the callee
	 * running in ACQUIRE or PROCESS */
	struct sar_user_call_data * 	user_call_data;
};


/* NSC functions */
uint32_t sar_entry_gateway(uint32_t req_phase, uint32_t duration, void * pc, void * user_call_data);
uint32_t sar_fullaccess_time_remaining(void);
uint32_t sar_wipe_all_zones(void);
uint32_t sar_app_maintain_buffers(uint32_t margin);


#endif /* INC_SAR_NS_H_ */
