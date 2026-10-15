/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body (Tetris 2P Logic + UART DMA to Python Hub)
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
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

/* USER CODE BEGIN PV */
typedef enum {
    STATE_IDLE,
    STATE_WAIT,
    STATE_ONE_PLAYER,
    STATE_TWO_PLAYER,
    STATE_PLAY,
    STATE_RESULT_SHOW
} GameState;

volatile GameState currentState = STATE_IDLE;
volatile uint8_t playerCount = 0;
volatile uint32_t waitTimer = 0;
const uint32_t WAIT_TIMEOUT_MS = 10000;

bool isGameOver = false;

uint8_t game_board[20][10] = {0};
uint8_t frame_sequence = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */
void send_join_packet(uint8_t player_id);
void send_skip_wait_packet(void);
void send_state_packet(uint8_t player_id, uint8_t seq, uint8_t raw_board[20][10],
                       uint8_t next_piece, uint8_t hold_piece, uint32_t score);
void send_gameover_packet(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

uint8_t calculate_crc8(uint8_t *data, uint16_t len) {
    uint8_t crc = 0x00;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) crc = (crc << 1) ^ 0x07;
            else crc = (crc << 1);
        }
    }
    return crc;
}

void send_join_packet(uint8_t player_id) {
    uint8_t packet[26] = {0};

    packet[0] = 0xAA;
    packet[1] = 0x01;
    packet[2] = 22;

    packet[3] = player_id;
    packet[4] = 0;

    packet[25] = calculate_crc8(&packet[1], 24);

    HAL_UART_Transmit(&huart3, packet, 26, 100);
}

void send_skip_wait_packet(void) {
    uint8_t packet[4] = {0};
    packet[0] = 0xAA;
    packet[1] = 0x05;
    packet[2] = 0;
    packet[3] = calculate_crc8(&packet[1], 2);

    HAL_UART_Transmit(&huart3, packet, 4, 100);
}

// ยิงคำสั่งจบเกมไปที่คอมพิวเตอร์
void send_gameover_packet(void) {
    uint8_t packet[4] = {0};
    packet[0] = 0xAA; // SYNC
    packet[1] = 0x04; // TYPE_GAMEOVER = 0x04
    packet[2] = 0;    // LEN
    packet[3] = calculate_crc8(&packet[1], 2);

    HAL_UART_Transmit(&huart3, packet, 4, 100);
}

void compress_board(uint8_t raw_board[20][10], uint8_t compressed[100]) {
    memset(compressed, 0, 100);
    for (int y = 0; y < 20; y++) {
        for (int x = 0; x < 10; x++) {
            int index = y * 10 + x;
            uint8_t cell = raw_board[y][x] & 0x0F;
            if (index % 2 == 0) compressed[index / 2] |= (cell << 4);
            else compressed[index / 2] |= cell;
        }
    }
}

void send_state_packet(uint8_t player_id, uint8_t seq, uint8_t raw_board[20][10],
                       uint8_t next_piece, uint8_t hold_piece, uint32_t score)
{
    static uint8_t packet[116] = {0};
    uint8_t payload[112] = {0};

    payload[0] = player_id;
    payload[1] = seq;
    compress_board(raw_board, &payload[2]);
    payload[102] = next_piece;
    payload[103] = hold_piece;

    payload[104] = (uint8_t)(score & 0xFF);
    payload[105] = (uint8_t)((score >> 8) & 0xFF);
    payload[106] = (uint8_t)((score >> 16) & 0xFF);
    payload[107] = (uint8_t)((score >> 24) & 0xFF);

    payload[108] = 0; payload[109] = 0;
    payload[110] = 1;
    payload[111] = 0;

    packet[0] = 0xAA;
    packet[1] = 0x02;
    packet[2] = 112;

    memcpy(&packet[3], payload, 112);

    packet[115] = calculate_crc8(&packet[1], 114);

    HAL_UART_Transmit(&huart3, packet, 116, 100);
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

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* Enable the CPU Cache */

  /* Enable I-Cache---------------------------------------------------------*/
  SCB_EnableICache();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  MX_USART6_UART_Init();
  /* USER CODE BEGIN 2 */

  currentState = STATE_IDLE;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    // อ่านค่าปุ่มกดทั้ง 4 ปุ่ม (ต้องแก้ที่ gpio.c ให้เป็น GPIO_PULLUP)
    bool btn_red_join   = (HAL_GPIO_ReadPin(GPIOF, GPIO_PIN_13) == GPIO_PIN_RESET);
    bool btn_green_skip = (HAL_GPIO_ReadPin(GPIOF, GPIO_PIN_14) == GPIO_PIN_RESET);
    bool btn_3          = (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_9)  == GPIO_PIN_RESET);
    bool btn_4          = (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_11) == GPIO_PIN_RESET);

    switch (currentState) {

        case STATE_IDLE:
            if (btn_red_join) {
                playerCount = 1;
                waitTimer = HAL_GetTick();
                currentState = STATE_WAIT;

                send_join_packet(1);
                HAL_Delay(300);
            }
            break;

        case STATE_WAIT:
            // ส่งสัญญาณภาพย้ำหน้าจอทุกๆ 1 วินาที ป้องกันจอค้าง
            if ((HAL_GetTick() - waitTimer) % 1000 == 0) {
                 send_join_packet(1);
                 HAL_Delay(1);
            }

            if (btn_red_join) {
                playerCount = 2;
                send_join_packet(2);
                currentState = STATE_TWO_PLAYER;
                HAL_Delay(300);
            }
            else if (btn_green_skip) {
                send_skip_wait_packet();
                currentState = STATE_ONE_PLAYER;
                HAL_Delay(300);
            }
            else if (HAL_GetTick() - waitTimer >= 10000) {
                send_skip_wait_packet();
                currentState = STATE_ONE_PLAYER;
                HAL_Delay(300);
            }
            break;

        case STATE_ONE_PLAYER:
            memset(game_board, 0, sizeof(game_board));
            frame_sequence = 0;
            isGameOver = false;
            currentState = STATE_PLAY;
            break;

        case STATE_TWO_PLAYER:
            memset(game_board, 0, sizeof(game_board));
            frame_sequence = 0;
            isGameOver = false;
            currentState = STATE_PLAY;
            break;

        case STATE_PLAY:

            frame_sequence++;
            send_state_packet(1, frame_sequence, game_board, 1, 0, 0);

            // ==========================================
            // จำลองการจบเกม: ถ้ากดปุ่ม 3 หรือปุ่ม 4 ในขณะเล่น ให้เกมจบ
            // ==========================================
            if (btn_3 || btn_4) {
                isGameOver = true;
            }

            HAL_Delay(100);

            if (isGameOver) {
                send_gameover_packet(); // ยิงคำสั่งบอกจอว่าเกมจบ
                currentState = STATE_RESULT_SHOW;
                waitTimer = HAL_GetTick();
            }
            break;

        case STATE_RESULT_SHOW:
            // โชว์คะแนน 5 วินาที
            if (HAL_GetTick() - waitTimer >= 5000) {
                isGameOver = false;
                playerCount = 0;
                currentState = STATE_IDLE;
            }
            break;
    }

    HAL_Delay(10);
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

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 216;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_7) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  HAL_MPU_Disable();

  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
