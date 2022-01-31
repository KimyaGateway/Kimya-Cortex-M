/**
  ******************************************************************************
  * @file    network_data.c
  * @author  AST Embedded Analytics Research Platform
  * @date    Wed Oct 13 11:16:58 2021
  * @brief   AI Tool Automatic Code Generator for Embedded NN computing
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
#include "network_data.h"
#include "ai_platform_interface.h"



AI_API_DECLARE_BEGIN

/*!
 * @brief Get network weights array pointer as a handle ptr.
 * @ingroup network_data
 * @return a ai_handle pointer to the weights array
 */
AI_DEPRECATED
AI_API_ENTRY
ai_handle ai_network_data_weights_get(void)
{
  return AI_HANDLE_PTR(NULL);
}


/*!
 * @brief Get network params configuration data structure.
 * @ingroup network_data
 * @return true if a valid configuration is present, false otherwise
 */
AI_API_ENTRY
ai_bool ai_network_data_params_get(ai_handle network, ai_network_params* params)
{
  if (!(network && params)) return false;
  
  static ai_buffer s_network_data_map_activations[AI_NETWORK_DATA_ACTIVATIONS_COUNT] = {
    AI_BUFFER_OBJ_INIT(AI_BUFFER_FORMAT_U8, 1, 1, AI_NETWORK_DATA_ACTIVATIONS_SIZE, 1, NULL)
  };

  const ai_buffer_array map_activations = 
    AI_BUFFER_ARRAY_OBJ_INIT(AI_FLAG_NONE, AI_NETWORK_DATA_ACTIVATIONS_COUNT, s_network_data_map_activations);
  
  static ai_buffer s_network_data_map_weights[AI_NETWORK_DATA_WEIGHTS_COUNT] = {
    AI_BUFFER_OBJ_INIT(AI_BUFFER_FORMAT_U8, 1, 1, 41816, 1, NULL),
    
  };

  const ai_buffer_array map_weights = 
    AI_BUFFER_ARRAY_OBJ_INIT(AI_FLAG_NONE, AI_NETWORK_DATA_WEIGHTS_COUNT, s_network_data_map_weights);

  return ai_platform_bind_network_params(network, params, &map_weights, &map_activations);
}


AI_API_DECLARE_END
