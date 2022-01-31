#ifndef _JTEST_CYCLE_H_
#define _JTEST_CYCLE_H_

/*--------------------------------------------------------------------------------*/
/* Includes */
/*--------------------------------------------------------------------------------*/

#include "../../../../../CMSIS_DSP/DSP_Lib_TestSuite/Common/JTest/inc/jtest_fw.h"           /* JTEST_DUMP_STRF() */
#include "../../../../../CMSIS_DSP/DSP_Lib_TestSuite/Common/JTest/inc/jtest_systick.h"
#include "../../../../../CMSIS_DSP/DSP_Lib_TestSuite/Common/JTest/inc/jtest_util.h"         /* STR() */

/*--------------------------------------------------------------------------------*/
/* Declare Module Variables */
/*--------------------------------------------------------------------------------*/
extern const char * JTEST_CYCLE_STRF;

/*--------------------------------------------------------------------------------*/
/* Macros and Defines */
/*--------------------------------------------------------------------------------*/

/**
 *  Wrap the function call, fn_call, to count execution cycles and display the
 *  results.
 */
/* skipp function name + param
#define JTEST_COUNT_CYCLES(fn_call)                     \
    do                                                  \
    {                                                   \
        uint32_t __jtest_cycle_end_count;               \
                                                        \
        JTEST_SYSTICK_RESET(SysTick);                   \
        JTEST_SYSTICK_START(SysTick);                   \
                                                        \
        fn_call;                                        \
                                                        \
        __jtest_cycle_end_count =                       \
            JTEST_SYSTICK_VALUE(SysTick);               \
                                                        \
		JTEST_SYSTICK_RESET(SysTick);                   \
        JTEST_DUMP_STRF(JTEST_CYCLE_STRF,               \
                        STR(fn_call),                   \
                        (JTEST_SYSTICK_INITIAL_VALUE -  \
                         __jtest_cycle_end_count));     \
    } while (0)
*/
#ifndef ARMv7A

#define JTEST_COUNT_CYCLES(fn_call)                     \
    do                                                  \
    {                                                   \
        uint32_t __jtest_cycle_end_count;               \
                                                        \
        JTEST_SYSTICK_RESET(SysTick);                   \
        JTEST_SYSTICK_START(SysTick);                   \
                                                        \
        fn_call;                                        \
                                                        \
        __jtest_cycle_end_count =                       \
            JTEST_SYSTICK_VALUE(SysTick);               \
                                                        \
		JTEST_SYSTICK_RESET(SysTick);           \
        JTEST_DUMP_STRF(JTEST_CYCLE_STRF,               \
                        (JTEST_SYSTICK_INITIAL_VALUE -  \
                         __jtest_cycle_end_count));     \
    } while (0)

#else
/* TODO */
#define JTEST_COUNT_CYCLES(fn_call)                     \
    do                                                  \
    {                                                   \
		fn_call;   										\
    } while (0)

#endif

#endif /* _JTEST_CYCLE_H_ */


