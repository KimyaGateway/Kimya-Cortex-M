#ifndef INC_SPEECH_PARAMS_H_
#define INC_SPEECH_PARAMS_H_

#include "arm_math.h"

#define SP_FRAME_LEN 1024
#define SP_FRAME_STEP 1024
#define SP_SAMPLE_RATE 1024
#define SP_MEL_LOWER_HZ 160
#define SP_MEL_UPPER_HZ 6000
#define SP_MEL_NUM_BINS 13

const static uint16_t sp_mel_points[SP_MEL_NUM_BINS + 1] = {10,19,30,43,58,76,97,121,149,183,222,267,321,384};

#endif   /* INC_SPEECH_PARAMS_H_ */
