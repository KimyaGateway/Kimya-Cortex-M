/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "sar_ns.h"
#include "mic_ns.h"
#include "arm_math.h"
#include "audio.h"
#include "ai.h"
#include "printf.h"
#include "profiling.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define CONTAINER_PRINTF_ACCESS 0

#if CONTAINER_PRINTF_ACCESS == 1
	#define CONTAINER_PRINTF printf_
	#define CONTAINER_PRINTF_MARGIN 2000
#else
	#define CONTAINER_PRINTF(...)
	#define CONTAINER_PRINTF_MARGIN 0
#endif

//#define FULL_ACCESS_DUMP_DATA


#define PROACTIVE_BUFFER_MAINTENANCE
#define PROACTIVE_NETWORK_INIT

// Note: not all combinations are properly covered here.
#ifdef OSPEED
	#define CONTAINER_TIME_ACQUIRE_MEL 990
	#define CONTAINER_TIME_PROCESS_RUN_NN 15790
#else
	#ifdef OSIZE
		#ifdef PROACTIVE_NETWORK_INIT
			#define CONTAINER_TIME_ACQUIRE_MEL 1080
			#define CONTAINER_TIME_PROCESS_RUN_NN 15610
			#define CONTAINER_TIME_PROCESS_INIT_NN 260
		#else
			#define CONTAINER_TIME_ACQUIRE_MEL 1070
			#define CONTAINER_TIME_PROCESS_RUN_NN 15854
		#endif
	#else
		#define CONTAINER_TIME_ACQUIRE_MEL 3700
		#define CONTAINER_TIME_PROCESS_RUN_NN 16050
	#endif /* ifdef OSIZE */
#endif /* ifdef OSPEED */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

CRC_HandleTypeDef hcrc;

UART_HandleTypeDef hlpuart1;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_RTC_Init(void);
static void MX_CRC_Init(void);
static void MX_LPUART1_UART_Init(void);
/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/********************
 * Demo Application *
 ********************
 *
 * This file implements a SAR demo application where in the ACQUIRE phase
 * MEL speech features are actracted from the microphone audio.
 * In the PROCESS phase, a neural network inference is ran in order to detect
 * utterances of the keyword "cat".
 *
 * During the demo the LEDs have the following meanings:
 * LED_GREEN: FULLACCESS mode is active
 * LED_BLUE: Blinks whenever a SAR notification is sent
 * LED_RED: Lit when the keyword "cat" is detected
 */

static UART_HandleTypeDef *dumpUart = &hlpuart1;
static UART_HandleTypeDef *printfUart = &hlpuart1;

void _putchar(char character)
{
  // send char to console etc.
  while(!(printfUart->Instance->ISR & USART_ISR_TXE_TXFNF));
  printfUart->Instance->TDR = character;
}

uint32_t ACQUIRE_check_audio_threshold(struct sar_call_data *);
uint32_t ACQUIRE_extract_speech_features(struct sar_call_data *);
uint32_t PROCESS_run_neural_network(struct sar_call_data *);

/*
 * Demo function to be run in SAR_PHASE_ACQUIRE. Looks for any load noises in the
 * microphone data.
 */
uint32_t ACQUIRE_check_audio_threshold(struct sar_call_data * call_data){

	struct mic_memory_zone_t *mic_buffer = (struct mic_memory_zone_t *) MIC_BUFFER_LOCATION;

	int16_t max, min;
	uint32_t devnul;
	arm_max_q15((q15_t *) mic_buffer->active_buffer, MIC_BUFFER_LEN, (q15_t *) &max, &devnul);
	arm_min_q15((q15_t *) mic_buffer->active_buffer, MIC_BUFFER_LEN, (q15_t *) &min, &devnul);

	if (max - min > 0xFFF){
		return SAR_PHASE_FULLACCESS;
	}

	return SAR_PHASE_IDLE;
}

/*
 * Calculate Mel coefficients of input data and add to the buffer in the acquire heap.
 */
uint32_t ACQUIRE_extract_speech_features(struct sar_call_data *call_data){

	PROF_WRITE(PROF_POINT_ACQUIRE_MEL_START);

	struct mic_memory_zone_t *mic_buffer = (struct mic_memory_zone_t *) MIC_BUFFER_LOCATION;
	struct audio_acquire_heap_t * heap = (struct audio_acquire_heap_t *) call_data->active_buffer;

	if (heap->mel_count == 0){
		audio_calc_init(heap);
	}
	audio_calc_mel_coefficients(mic_buffer->active_buffer, heap);

	if (heap->mel_count < AUDIO_MEL_ROLING_BUFFER_SIZE){
		heap->mel_count++;
	}

	PROF_WRITE(PROF_POINT_ACQUIRE_MEL_END);
	return SAR_PHASE_IDLE;
}

uint32_t PROCESS_init_neural_network(struct sar_call_data *call_data){

	PROF_WRITE(PROF_POINT_PROCESS_NN_INIT_START);


	struct ai_heap_t *heap = (struct ai_heap_t *) SAR_SCRATCH_BASE;
	/* If the network in uninitialized, initialize it */
	if (heap->network == 0){
		ai_init(heap);
		CONTAINER_PRINTF("Init NN (1)\n");
	}

	PROF_WRITE(PROF_POINT_PROCESS_NN_INIT_END);
	return SAR_PHASE_IDLE;
}

/*
 * Prepare MEL coefficients for inference and run the neural network
 */
uint32_t PROCESS_run_neural_network(struct sar_call_data *call_data){

	PROF_WRITE(PROF_POINT_PROCESS_NN_START);


	struct ai_heap_t *heap = (struct ai_heap_t *) SAR_SCRATCH_BASE;

	/* Figure out which acquire heap is active and inactive */
	struct audio_acquire_heap_t *active_acquire_heap =
			(struct audio_acquire_heap_t *) call_data->active_buffer;
	struct audio_acquire_heap_t *inactive_acquire_heap;

	if (active_acquire_heap == (struct audio_acquire_heap_t *) SAR_BUFFER1_BASE){
		inactive_acquire_heap = (struct audio_acquire_heap_t *) SAR_BUFFER2_BASE;
	}
	else{
		inactive_acquire_heap = (struct audio_acquire_heap_t *) SAR_BUFFER1_BASE;
	}

	uint32_t data_available = ai_prepare_mels(active_acquire_heap, inactive_acquire_heap, heap);


	/* If we do not yet have enough audio frames to run interference, don't make a prediction */
	if(!data_available){
		CONTAINER_PRINTF("NN,0,     -1,0\n");
		return SAR_PHASE_IDLE;
	}

	/* If the network in uninitialized, initialize it */
	if (heap->network == 0){
		ai_init(heap);
		CONTAINER_PRINTF("Init NN (2)\n");
	}

	PROF_WRITE(PROF_POINT_PROCESS_NN_RUN);
	/* Run the ai network and report on the results */
	MACRO_SET_DEBUG_1();
	ai_run(heap->ai_in_data, heap->ai_out_data, heap);
	MACRO_RESET_DEBUG_1();

	float32_t nn_prediction = heap->ai_out_data[1];
	uint32_t __attribute__((unused)) nn_print = nn_prediction * 1e6;
	CONTAINER_PRINTF("NN,0,%7u", nn_print);

	if (nn_prediction > 0.5){
		CONTAINER_PRINTF(",1\n");
		PROF_WRITE(PROF_POINT_PROCESS_NN_END_KW);
		return SAR_PHASE_FULLACCESS;
	} else {
		CONTAINER_PRINTF(",0\n");
	}

	PROF_WRITE(PROF_POINT_PROCESS_NN_END_NOKW);
	return SAR_PHASE_IDLE;

}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_RTC_Init();
  MX_CRC_Init();
  MX_LPUART1_UART_Init();
  /* USER CODE BEGIN 2 */

#ifdef PROACTIVE_NETWORK_INIT
	__set_FAULTMASK(0x1);
	sar_entry_gateway(
		  SAR_PHASE_PROCESS,
		  CONTAINER_TIME_PROCESS_INIT_NN + CONTAINER_PRINTF_MARGIN,
		  PROCESS_init_neural_network,
		  NULL
	);
	__set_FAULTMASK(0x0);
#endif /* PROACTIVE_NETWORK_INIT */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  printf_("===ENTERING INFINITE LOOP===\n");
  uint32_t counter = 0;
  while (1)
  {
	  uint32_t time_remaining = sar_fullaccess_time_remaining();

	  /* code to run in the IDLE phase */
	  if(!time_remaining){
		  PROF_WRITE(PROF_POINT_NS_IDLE_START);
		  uint32_t sar_phase;
		  /* If there is new data, sample the microphone.
		   * Set FAULTMASK to prevent interrupts from triggering and
		   * accessing memory regions not allowed during SAR_PHASE_ACQUIRE */
		  uint32_t new_data = mic_check_for_new_data();
		  if (new_data){

			  MACRO_SET_WORKING();

			  PROF_WRITE(PROF_POINT_NS_NEWDATA_START);
			  __set_FAULTMASK(0x1);
			  sar_phase = sar_entry_gateway(
					  SAR_PHASE_ACQUIRE,
					  CONTAINER_TIME_ACQUIRE_MEL + CONTAINER_PRINTF_MARGIN,
					  ACQUIRE_extract_speech_features,
					  NULL
			  );
			  __set_FAULTMASK(0x0);

			  PROF_WRITE(PROF_POINT_NS_PROCESS_NN_ENTRY);

			  __set_FAULTMASK(0x1);
			  sar_phase = sar_entry_gateway(
					  SAR_PHASE_PROCESS,
					  CONTAINER_TIME_PROCESS_RUN_NN + CONTAINER_PRINTF_MARGIN,
					  PROCESS_run_neural_network,
					  NULL
			  );
			  __set_FAULTMASK(0x0);

			  PROF_WRITE(PROF_POINT_NS_RESULT_CHECK);

			  if(sar_phase == SAR_PHASE_FULLACCESS){
				  /* set RED led to indicate that Keyword was detected*/
				  MACRO_SET_KW_DETECTED();
				  LED_RED_GPIO_Port->BSRR = LED_RED_Pin;
			  } else {
				  /* set RED led to indicate that Keyword not detected*/
				  MACRO_RESET_KW_DETECTED();
				  LED_RED_GPIO_Port->BSRR = LED_RED_Pin << 16;
			  }

#ifdef PROACTIVE_BUFFER_MAINTENANCE
			  if (sar_app_maintain_buffers(100)){
#ifdef PROACTIVE_NETWORK_INIT

				  PROF_WRITE(PROF_POINT_NS_PROCESS_INIT_ENTRY);
				  __set_FAULTMASK(0x1);
				  sar_entry_gateway(
						  SAR_PHASE_PROCESS,
						  CONTAINER_TIME_PROCESS_INIT_NN + CONTAINER_PRINTF_MARGIN,
						  PROCESS_init_neural_network,
						  NULL
				  );
				  __set_FAULTMASK(0x0);
#endif /* PROACTIVE_NETWORK_INIT */
			  }
#endif /* PROACTIVE_BUFFER_MAINTENANCE */
			 PROF_WRITE(PROF_POINT_NS_NEWDATA_END);
		  }
		  else {
			  MACRO_RESET_WORKING();
		  }

	  /* code to run in the FULLACCESS phase */
#ifdef FULL_ACCESS_DUMP_DATA
	  } else if (time_remaining < 5000) {
		  sar_entry_gateway(SAR_PHASE_IDLE, 0, NULL, NULL);
		  PROF_WRITE(PROF_POINT_NS_FULLACCESS_END);
	  } else {
		  PROF_WRITE(PROF_POINT_NS_FULLACCESS_START);

		  if (mic_check_for_new_data()){
				uint8_t separator[5] = {10,68,69,76,33};
				HAL_UART_Transmit(dumpUart, (uint8_t *) &separator, 5, 100);

				struct mic_memory_zone_t *mic_memory_zone =  (struct mic_memory_zone_t *) MIC_BUFFER_LOCATION;
				HAL_UART_Transmit(dumpUart, (uint8_t *) mic_memory_zone->active_buffer, 2*MIC_BUFFER_LEN, 100);
		  }
#else
	  } else {
		  PROF_WRITE(PROF_POINT_NS_FULLACCESS_START);
		  sar_entry_gateway(SAR_PHASE_IDLE, 0, NULL, NULL);
		  PROF_WRITE(PROF_POINT_NS_FULLACCESS_END);
	  }
#endif

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_MultiModeTypeDef multimode = {0};
  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */
  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc1.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure the ADC multi-mode
  */
  multimode.Mode = ADC_MODE_INDEPENDENT;
  if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_3;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief CRC Initialization Function
  * @param None
  * @retval None
  */
static void MX_CRC_Init(void)
{

  /* USER CODE BEGIN CRC_Init 0 */

  /* USER CODE END CRC_Init 0 */

  /* USER CODE BEGIN CRC_Init 1 */

  /* USER CODE END CRC_Init 1 */
  hcrc.Instance = CRC;
  hcrc.Init.DefaultPolynomialUse = DEFAULT_POLYNOMIAL_ENABLE;
  hcrc.Init.DefaultInitValueUse = DEFAULT_INIT_VALUE_ENABLE;
  hcrc.Init.InputDataInversionMode = CRC_INPUTDATA_INVERSION_NONE;
  hcrc.Init.OutputDataInversionMode = CRC_OUTPUTDATA_INVERSION_DISABLE;
  hcrc.InputDataFormat = CRC_INPUTDATA_FORMAT_BYTES;
  if (HAL_CRC_Init(&hcrc) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CRC_Init 2 */

  /* USER CODE END CRC_Init 2 */

}

/**
  * @brief LPUART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_LPUART1_UART_Init(void)
{

  /* USER CODE BEGIN LPUART1_Init 0 */

  /* USER CODE END LPUART1_Init 0 */

  /* USER CODE BEGIN LPUART1_Init 1 */

  /* USER CODE END LPUART1_Init 1 */
  hlpuart1.Instance = LPUART1;
  hlpuart1.Init.BaudRate = 2000000;
  hlpuart1.Init.WordLength = UART_WORDLENGTH_8B;
  hlpuart1.Init.StopBits = UART_STOPBITS_1;
  hlpuart1.Init.Parity = UART_PARITY_NONE;
  hlpuart1.Init.Mode = UART_MODE_TX_RX;
  hlpuart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  hlpuart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  hlpuart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  hlpuart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  hlpuart1.FifoMode = UART_FIFOMODE_ENABLE;
  if (HAL_UART_Init(&hlpuart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&hlpuart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&hlpuart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_EnableFifoMode(&hlpuart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN LPUART1_Init 2 */

  /* USER CODE END LPUART1_Init 2 */

}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  HAL_PWREx_EnableVddIO2();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, MACRO_KW_DETECTED_Pin|MACRO_WIPING_Pin|MACRO_WORKING_Pin|MACRO_DBG_1_Pin
                          |LED_RED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, PROF0_Pin|PROF1_Pin|PROF2_Pin|PROF3_Pin
                          |PROF4_Pin|PROF5_Pin|PROF6_Pin|PROF7_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : MACRO_KW_DETECTED_Pin MACRO_WIPING_Pin MACRO_WORKING_Pin MACRO_DBG_1_Pin */
  GPIO_InitStruct.Pin = MACRO_KW_DETECTED_Pin|MACRO_WIPING_Pin|MACRO_WORKING_Pin|MACRO_DBG_1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : LED_RED_Pin */
  GPIO_InitStruct.Pin = LED_RED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_RED_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PROF0_Pin PROF1_Pin PROF2_Pin PROF3_Pin
                           PROF4_Pin PROF6_Pin PROF7_Pin */
  GPIO_InitStruct.Pin = PROF0_Pin|PROF1_Pin|PROF2_Pin|PROF3_Pin
                          |PROF4_Pin|PROF6_Pin|PROF7_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pin : PROF5_Pin */
  GPIO_InitStruct.Pin = PROF5_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(PROF5_GPIO_Port, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */

  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
