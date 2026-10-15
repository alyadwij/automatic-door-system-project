/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#define GPIO_SET_PIN(port, pin) ((port)->BSRR = (pin))
#define GPIO_CLEAR_PIN(port, pin) ((port)->BSRR = (pin << 16u))

/*#define SYSTICK_LOAD (SystemCoreClock/1000000U)
#define SYSTICK_DELAY_CALIB (SYSTICK_LOAD >> 1)

#define DELAY_US(us)\
	do{\
		uint32_t start = SysTick->VAL;\
		uint32_t ticks = (us * SYSTICK_LOAD)-SYSTICK_DELAY_CALIB;\
		while((start - SysTick->VAL) < ticks);\
	}while (0) */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
IWDG_HandleTypeDef hiwdg;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;

/* USER CODE BEGIN PV */
uint32_t start_time = 0, end_time = 0;
uint8_t capture_done = 0;
float distance;

uint32_t pintu_timer = 0;
uint32_t buka_timer = 0;
uint32_t ultra_timer = 0;
static uint16_t pulse = 800;

uint32_t beep_until = 0;
uint32_t wd_led_until = 0;
uint8_t wd_reset_flag = 0;
// State
typedef enum{
	IDLE_TUTUP = 0,
	BUKA,
	STAY,
	TUTUP,
	TUNGGU_TUTUP,
	ERROR_PINTU
} state_pintu;

state_pintu state = IDLE_TUTUP;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_IWDG_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
/* USER CODE BEGIN PFP */
void trigger_ultrasonik(void);
void servo_buka(void);
void servo_tutup(void);
void bunyi(uint16_t dur_ms);
float baca_jarak(void);
uint8_t cek_pintu(void);
void handle_state_pintu(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static inline void DELAY_US(uint32_t us)
{
    uint32_t start = __HAL_TIM_GET_COUNTER(&htim2);
    uint32_t target = (start + us) & 0xFFFFu;
    if (start <= target) {
        while (__HAL_TIM_GET_COUNTER(&htim2) < target) { }
    } else {
        while (__HAL_TIM_GET_COUNTER(&htim2) >= start) { }
        while (__HAL_TIM_GET_COUNTER(&htim2) < target) { }
    }
}

void trigger_ultrasonik(void)
{
	GPIO_SET_PIN(GPIOA, TRIG_Pin);
	DELAY_US(10);
	GPIO_CLEAR_PIN(GPIOA, TRIG_Pin);
}

float baca_jarak(void)
{
	capture_done = 0;
	trigger_ultrasonik();
	uint32_t timeout = HAL_GetTick();
	uint32_t durasi;

	while(!capture_done){
		if(HAL_GetTick() - timeout > 100)
			return 999.0f;
	}

	if(end_time >= start_time) durasi = end_time - start_time;
	else durasi = (htim2.Init.Period -start_time)+end_time;
	return (durasi * 0.0343f)/2.0f;
}

void servo_buka(void)
{
	pulse = 2000;
	__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pulse);
}

void servo_tutup(void)
{
	static uint32_t last_move = 0;

	if (HAL_GetTick()-last_move > 20){
		last_move = HAL_GetTick();

		if (pulse > 800) {
			pulse -= 10;
		}
		__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pulse);
	}
}

void bunyi(uint16_t dur_ms)
{
	GPIO_SET_PIN(GPIOA, BUZZ_Pin);
	beep_until = HAL_GetTick() + dur_ms;
}

uint8_t cek_pintu(void)
{
	return (HAL_GPIO_ReadPin(IR_GPIO_Port, IR_Pin)==GPIO_PIN_SET);
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if(GPIO_Pin == ECHO_Pin){
		if(HAL_GPIO_ReadPin(ECHO_GPIO_Port, ECHO_Pin)==GPIO_PIN_SET){
			start_time = __HAL_TIM_GET_COUNTER(&htim2);
		}
		else
		{
			end_time = __HAL_TIM_GET_COUNTER(&htim2);
			capture_done = 1;
		}
	}
}

void handle_state_pintu(void)
{
	switch(state){
	  	  case IDLE_TUTUP:
	  		  buka_timer = 0;
	  		  if(distance>2 && distance<20.0f){
	  			  servo_buka();
	  			  bunyi(50);
	  			  state = BUKA;
	  		  }
	  		  break;
	  	  case BUKA:
	  		  if(buka_timer == 0){
	  			  buka_timer = HAL_GetTick();
	  		  }

	  		  if(pulse >= 1900){
	  			  buka_timer = 0;
	  			  pintu_timer = HAL_GetTick();
	  			  state = STAY;
	  			  break;
	  		  }
	  		  if(HAL_GetTick()-buka_timer>=500){
	  			  buka_timer = 0;
	  			  state = ERROR_PINTU;
	  			  break;
	  		  }
	  		  servo_buka();
	  		  break;
	  	  case STAY:
	  		  if(distance>2 && distance<20.0f){
	  			  servo_buka();
	  			  bunyi(50);
	  			  pintu_timer = HAL_GetTick();
	  		  }
	  		  if(HAL_GetTick() - pintu_timer >= 3000){
	  			  state = TUTUP;
	  		  }
	  		  break;
	  	  case TUTUP:
	  		  if(distance>2 && distance<= 20.0f){
	  			  servo_buka();
	  			  bunyi(50);
	  			  state = BUKA;
	  			  break;
	  		  }
	  		  servo_tutup();

	  		  if(pulse <= 2000)
	  			  bunyi(100);

	  		  if(pulse <= 800){
	  			  pintu_timer = HAL_GetTick();
	  			  state = TUNGGU_TUTUP;
	  		  }
	  		  break;
	  	  case TUNGGU_TUTUP:
	  		  if(HAL_GetTick() - pintu_timer >= 400){
	  			  if(cek_pintu()){
	  				  state = ERROR_PINTU;
	  			  } else {
	  				  state = IDLE_TUTUP;
	  			  }
	  		  }
	  		  break;
	  	  case ERROR_PINTU:
	  		  break;
	  }
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

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  if(__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST)){
	  wd_reset_flag = 1;

	  GPIO_CLEAR_PIN(GPIOC, GPIO_PIN_13);
	  wd_led_until = HAL_GetTick() + 500;
  }
  __HAL_RCC_CLEAR_RESET_FLAGS();
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_IWDG_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
  HAL_TIM_Base_Start(&htim2);

  ultra_timer = HAL_GetTick();

  pulse = 800;
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pulse);
  state = IDLE_TUTUP;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  if(wd_reset_flag && wd_led_until && HAL_GetTick()>=wd_led_until){
		  GPIO_SET_PIN(GPIOC, GPIO_PIN_13);
		  wd_led_until = 0;
		  wd_reset_flag = 0;
	  }

	  if (state != ERROR_PINTU){
		  HAL_IWDG_Refresh(&hiwdg);
	  }

	  if(HAL_GetTick() - ultra_timer >= 100){
		  ultra_timer = HAL_GetTick();
		  distance = baca_jarak();
	  }

	  if (beep_until && HAL_GetTick() >= beep_until) {
	      GPIO_CLEAR_PIN(GPIOA, BUZZ_Pin);
	      beep_until = 0;
	  }

	  handle_state_pintu();
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief IWDG Initialization Function
  * @param None
  * @retval None
  */
static void MX_IWDG_Init(void)
{

  /* USER CODE BEGIN IWDG_Init 0 */

  /* USER CODE END IWDG_Init 0 */

  /* USER CODE BEGIN IWDG_Init 1 */

  /* USER CODE END IWDG_Init 1 */
  hiwdg.Instance = IWDG;
  hiwdg.Init.Prescaler = IWDG_PRESCALER_32;
  hiwdg.Init.Reload = 4095;
  if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN IWDG_Init 2 */

  /* USER CODE END IWDG_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 71;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 65535;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 71;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 20000-1;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 1500;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */
  HAL_TIM_MspPostInit(&htim3);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, TRIG_Pin|BUZZ_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : ECHO_Pin */
  GPIO_InitStruct.Pin = ECHO_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(ECHO_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : TRIG_Pin BUZZ_Pin */
  GPIO_InitStruct.Pin = TRIG_Pin|BUZZ_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : IR_Pin */
  GPIO_InitStruct.Pin = IR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(IR_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI1_IRQn);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
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
  __disable_irq();
  while (1)
  {
  }
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
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
