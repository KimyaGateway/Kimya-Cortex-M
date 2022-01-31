/*
 * sar_s.h
 *
 *  Created on: Feb 28, 2021
 *      Author: AUTHOR_NAME_REMOVED
 */

#ifndef INC_SAR_S_H_
#define INC_SAR_S_H_

#include <stdint.h>

/* Set to allow ACQUIRE and PROCESS phases to access the UART */
//#define SAR_DEBUG_UART 1
//#define SAR_IGNORE_CONTAINER_TIMER
//#define SAR_NO_MPU


/* Offset between secure and Non-secure memory locations */
#define SAR_NS_S_SRAM_OFFSET 0x10000000
#define SAR_ptr_to_s(ptr) ((typeof(*ptr) *) ((uintptr_t)ptr | SAR_NS_S_SRAM_OFFSET))
#define SAR_ptr_to_ns(ptr) ((typeof(*ptr) *) ((uintptr_t)ptr & ~SAR_NS_S_SRAM_OFFSET))

/* defines for the timer used to signal max running time to
 * the callee functions in ACQUIRE or PROCESS */
#define SAR_CONTAINER_TIMER_TIM TIM7
#define SAR_CONTAINER_TIMER_PRESCALER 110 // resulting frequency 1 MHz
#define SAR_CONTAINER_TIMER_PERIOD 1 // us

/* defines for the timer used to enforce max running time of
 * FULLACCESS mode */
#define SAR_FULLACCESS_TIMER TIM6
#define SAR_FULLACCESS_TIMER_PRESCALER 55000 // resulting frequency 2 kHz
#define SAR_FULLACCESS_TIMER_PERIOD 500 // us
#define SAR_FULLACCESS_TIMER_MAXTIME 5000000 // us
/* The minus one is needed because the counter only overflows at value ARR + 1 */
#define SAR_FULLACCESS_TIMER_ARR ((SAR_FULLACCESS_TIMER_MAXTIME / SAR_FULLACCESS_TIMER_PERIOD - 1))


/* Define just used for bookkeeping */
#define SAR_NOTIFICATION_TIMER TIM4

/* defines for error signaling */
#define SAR_ERROR_PIN				LED_RED_Pin
#define SAR_ERROR_GPIO_Port 		LED_RED_GPIO_Port
#define SAR_ERROR_ON_TIME			1100000
#define SAR_ERROR_OFF_TIME      	5500000
#define SAR_ERROR_SPACE_TIME    	((3 * 5500000))

#define SAR_ERROR_CODE_CONTAINER_TIMER 			2
#define SAR_ERROR_CODE_FULLACCESS_TIMER			3
#define SAR_ERROR_CODE_WRONG_EXIT_PHASE     	4
#define SAR_ERROR_CODE_CONTAINER_REQ_TOO_LONG	5

#define SAR_ERROR_CODE_NOTIFICATION_FAILED		10
#define SAR_ERROR_CODE_BUFFER					11
#define SAR_ERROR_CODE_MPU_1					12
#define SAR_ERROR_CODE_MPU_2					13
#define SAR_ERROR_CODE_MPU_3					14
#define SAR_ERROR_CODE_MPU_FULL					15
#define SAR_ERROR_CODE_GATEWAY_BOTTOM       	16


/* MPU configuration registers */
#define MPU_REGION_COUNT 8
static uint32_t * const MPU_TYPE_NS 	= (void *) 0xE002ED90;
static uint32_t * const MPU_CTRL_NS 	= (void *) 0xE002ED94;
static uint32_t * const MPU_RNR_NS  	= (void *) 0xE002ED98;
static uint32_t * const MPU_RBAR_NS    	= (void *) 0xE002ED9C;
static uint32_t * const MPU_RLAR_NS    	= (void *) 0xE002EDA0;
static uint32_t * const MPU_MAIR0_NS   	= (void *) 0xE002EDC0;
static uint32_t * const MPU_MAIR1_NS   	= (void *) 0xE002EDC4;

/* Types of memory zones, used for MPU configuration */
#define SAR_ZONE_TYPE_BUFFER 1
#define SAR_ZONE_TYPE_SCRATCH 2
#define SAR_ZONE_TYPE_CODE 4
#define SAR_ZONE_TYPE_PERIPHERAL 5
#define SAR_ZONE_TYPE_RAM 6

/* Read-Only or Read-Write access defines */
#define SAR_ACCESS_LEVEL_RO 1
#define SAR_ACCESS_LEVEL_RW 2


/* The SAR has a number of states that define
 * what the access levels of the non secure code are */

/* Idle: there is no access to sensors or buffers */
#define SAR_PHASE_IDLE 1
/* ACQUIRE: there is RW access to one of the two buffers.
 * The two buffers are alternating
 */
#define SAR_PHASE_ACQUIRE 2
/* Process: there is RO access to both buffers and RW
 * access to the scratch
 */
#define SAR_PHASE_PROCESS 3
/* FULLACCESS: full access to everything (requires notification) */
#define SAR_PHASE_FULLACCESS 4

/* defines how often buffer1 and buffer2 are rotated */
#define SAR_BUFFER_ROTATION_MS 1000
#define SAR_MAX_AQUIRE_PROCESS_US 100000

/* A bunch of memory locations */
static uint32_t * const SAR_BUFFER1_BASE 		= (void *) 0x20030000;
static uint32_t * const SAR_BUFFER1_TOP 		= (void *) 0x20033fff;
static uint32_t * const SAR_BUFFER2_BASE 		= (void *) 0x20034000;
static uint32_t * const SAR_BUFFER2_TOP 		= (void *) 0x20037fff;
static uint32_t * const SAR_SCRATCH_BASE 		= (void *) 0x20038000;
static uint32_t * const SAR_SCRATCH_TOP 		= (void *) 0x2003bfff;
static uint32_t * const SAR_VIRTMIC_BASE 		= (void *) 0x2003c000;
static uint32_t * const SAR_VIRTMIC_TOP 		= (void *) 0x2003dfff;
static uint32_t * const SAR_SPARE_BASE 			= (void *) 0x2003e000;
static uint32_t * const SAR_SPARE_TOP 			= (void *) 0x2003ffff;

static uint32_t * const FLASH_S_CODE_BASE 	= (void *) 0x0C000000;
static uint32_t * const FLASH_S_CODE_TOP	= (void *) 0x0C03DFFF;
static uint32_t * const FLASH_NSC_CODE_BASE = (void *) 0x0C03E000;
static uint32_t * const FLASH_NSC_CODE_TOP 	= (void *) 0x0C03FFFF;
static uint32_t * const FLASH_NS_CODE_BASE 	= (void *) 0x08040000;
static uint32_t * const FLASH_NS_CODE_TOP 	= (void *) 0x0807FFFF;

static uint32_t * const NS_RAM_BASE 	= (void *) 0x20018000;
static uint32_t * const NS_RAM_TOP 		= (void *) 0x2002FFFF;

static uint32_t * const SAR_TIMER_BASE		= (void *) 0x40001400;
static uint32_t * const SAR_TIMER_TOP		= (void *) 0x400017FF;

static uint32_t * const SAR_CRC_BASE 	= (void *) 0x40023000;
static uint32_t * const SAR_CRC_TOP 	= (void *) 0x400233FF;
static uint32_t * const SAR_UART_BASE 	= (void *) 0x40008000;
static uint32_t * const SAR_UART_TOP 	= (void *) 0x400083FF;
static uint32_t * const SAR_GPIOD_BASE  = (void *) 0x42020C00;
static uint32_t * const SAR_GPIOD_TOP   = (void *) 0x42020FFF;
static uint32_t * const SAR_GPIOA_BASE  = (void *) 0x42020000;
static uint32_t * const SAR_GPIOA_TOP   = (void *) 0x420203FF;


/*
 * Struct to hold information about memory zones.
 *
 * Currently top and top_max are always the same. The idea
 * is to allow the non-secure application to dynamically
 * change the size of BUFFER and SCRATCH regions
 * so that these regions takes less time.
 *
 * The following holds at all times for the SAR zones:
 * 	1. No data in the zone is ever older than the "oldest data" timestamp
 * 	2. The range (top, top_max] is always zeroed
 */
struct sar_zone_t {
	void  *		base;
	void  *		top;
	void  *		top_max;
	uint32_t 	type;
	uint32_t 	oldest_data;
};

/* these structures hold information about the state of each memory region */
struct sar_zone_t sar_buffer1, sar_buffer2, sar_scratch, sar_code,
				sar_nsram, sar_timer, sar_virtmic, sar_crc, sar_uart,
				sar_gpiod, sar_gpioa;
struct sar_zone_t * sar_active_buffer;

/* struct that is passed to callee functions in ACQUIRE and PROCESS */
struct sar_call_data {
	uint32_t *  					timer_value;
	uint32_t						max_time;
	uint32_t *  					active_buffer;
	void * 							user_call_data;
};



uint32_t __attribute__((cmse_nonsecure_entry)) sar_entry_gateway(uint32_t req_phase, uint32_t duration, void * pc, void * user_call_data);
uint32_t __attribute__((cmse_nonsecure_entry)) sar_fullaccess_time_remaining(void);
uint32_t __attribute__((cmse_nonsecure_entry)) sar_wipe_all_zones(void);
uint32_t __attribute__((cmse_nonsecure_entry)) sar_app_maintain_buffers(uint32_t margin);


void sar_init(void);
void sar_prepare_for_idle(void);
void *sar_prepare_for_acquire(void);
void *sar_prepare_for_process(void);
void sar_prepare_for_fullaccess(void);
uint32_t sar_nonsecure_call(void * call_data , void * pc, void * sp) __attribute__((naked));


uint32_t sar_maintain_buffers(uint32_t margin);
void sar_wipe_memory_region(uint32_t *start, uint32_t* end);
void sar_wipe_zone(struct sar_zone_t * zone, uint32_t time);

void sar_fullaccess_timer_init(void);
void sar_fullaccess_timer_start(void);
void sar_fullaccess_timer_stop(void);

void sar_container_timer_init(void);
void sar_container_timer_start(void);
void sar_container_timer_stop(void);
uint32_t sar_wait_for_container_timer(uint32_t target);

void sar_notification_init(void);
uint32_t sar_notification_do(void);

void sar_error_delay(uint32_t count);
void sar_error_forever_loop(uint32_t error_code);

void mpu_enable_ns(void);
void mpu_disable_ns(void);
void mpu_clear_ns(void);
void mpu_grant_access(struct sar_zone_t * zone, uint32_t access_level, uint32_t mpu_region_nr);

void watchdog_start(void);
void watchdog_reset(void);

#endif /* INC_SAR_S_H_ */
