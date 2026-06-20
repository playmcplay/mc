/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : 贪吃蛇 FreeRTOS 满血版 (含死亡、复活、音效、红外解码)
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "tim.h"
#include "gpio.h"
#include "fsmc.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
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
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// ==========================================================
// 贪吃蛇游戏引擎核心逻辑
// ==========================================================
#include "lcd.h"
#include "delay.h"
#include <stdlib.h>  // 用于 rand() 随机数生成

#define BLOCK_SIZE 10   // 蛇身和食物的大小 (10x10像素)
#define MAX_X 240       // 屏幕宽度
#define MAX_Y 320       // 屏幕高度
#define MAX_LEN 100     // 蛇的最大长度

// 定义方向枚举
typedef enum { UP, DOWN, LEFT, RIGHT } Dir;

// 定义蛇的结构体
struct {
    int x[MAX_LEN];
    int y[MAX_LEN];
    int len;
    Dir dir;
} snake;

int food_x, food_y;
int is_dead = 0;        // 游戏死亡标志位 (0: 活着, 1: 挂了)
int beep_cmd = 0;       // 蜂鸣器音效指令 (0: 安静, 1: 吃苹果单响, 2: 死亡双响)

// ==========================================================
// 红外遥控器 (NEC协议) 解码核心数据
// ==========================================================
uint8_t  IR_State = 0;       // 状态机标志 (0:等上升沿, 1:等下降沿)
uint32_t IR_Time = 0;        // 捕获到的脉宽时间 (单位:us)
uint32_t IR_Data_Temp = 0;   // 临时接收的数据
uint8_t  IR_Bit_Count = 0;   // 已接收的比特位数
uint8_t  IR_Data_Ready = 0;  // 一帧数据接收完成标志 (1:完成)
uint32_t IR_Final_Data = 0;  // 最终解码出的 32位 完整数据

// ⚠️ 注意：这里的键值需要根据你实际遥控器进行修改 (Debug模式看 IR_Final_Data 的值)
#define IR_BTN_UP    0x00FF629D  // "上/CH+" 键
#define IR_BTN_DOWN  0x00FFA857  // "下/CH-" 键
#define IR_BTN_LEFT  0x00FF22DD  // "左/|<<" 键
#define IR_BTN_RIGHT 0x00FFC23D  // "右/>>|" 键
#define IR_BTN_OK    0x00FF02FD  // "OK/播放" 键 (用于复活)

// 捕获中断回调函数：用于测量高电平脉宽
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM1 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {
        if (IR_State == 0) {
            __HAL_TIM_SET_COUNTER(htim, 0);
            __HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_1, TIM_INPUTCHANNELPOLARITY_FALLING);
            IR_State = 1;
        } else if (IR_State == 1) {
            IR_Time = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
            __HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_1, TIM_INPUTCHANNELPOLARITY_RISING);
            IR_State = 0;

            if (IR_Time > 4000 && IR_Time < 5000) {
                IR_Data_Temp = 0;
                IR_Bit_Count = 0;
            } else if (IR_Time > 1300 && IR_Time < 2000) {
                IR_Data_Temp = (IR_Data_Temp << 1) | 1;
                IR_Bit_Count++;
            } else if (IR_Time > 300 && IR_Time < 800) {
                IR_Data_Temp = (IR_Data_Temp << 1) | 0;
                IR_Bit_Count++;
            }

            if (IR_Bit_Count >= 32) {
                IR_Final_Data = IR_Data_Temp;
                IR_Data_Ready = 1;
                IR_Bit_Count = 0;
            }
        }
    }
}

void game_init(void);

// 基础画图函数
void draw_block(int x, int y, uint16_t color) {
    lcd_fill(x, y, x + BLOCK_SIZE - 1, y + BLOCK_SIZE - 1, color);
}

// 随机生成食物
void spawn_food() {
    food_x = (rand() % (MAX_X / BLOCK_SIZE)) * BLOCK_SIZE;
    food_y = (rand() % (MAX_Y / BLOCK_SIZE)) * BLOCK_SIZE;
    draw_block(food_x, food_y, GREEN);
}

// 物理按键与红外遥控综合扫描 (供 FreeRTOS 大厨二号调用)
void key_scan() {
    // 1. 处理红外遥控器输入
    if (IR_Data_Ready) {
        IR_Data_Ready = 0;
        if (is_dead) {
            if (IR_Final_Data == IR_BTN_OK) {
                game_init();
                return;
            }
        } else {
            if (IR_Final_Data == IR_BTN_UP && snake.dir != DOWN)         snake.dir = UP;
            else if (IR_Final_Data == IR_BTN_DOWN && snake.dir != UP)    snake.dir = DOWN;
            else if (IR_Final_Data == IR_BTN_LEFT && snake.dir != RIGHT) snake.dir = LEFT;
            else if (IR_Final_Data == IR_BTN_RIGHT && snake.dir != LEFT) snake.dir = RIGHT;
        }
    }

    // 2. 原有的物理按键输入 (双模控制)
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET) {
        if (is_dead) {
            game_init();
            return;
        }
        if (snake.dir != DOWN) snake.dir = UP;
    } else if (!is_dead) {
        if (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_3) == GPIO_PIN_RESET) {
            if (snake.dir != UP) snake.dir = DOWN;
        } else if (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_2) == GPIO_PIN_RESET) {
            if (snake.dir != RIGHT) snake.dir = LEFT;
        } else if (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_4) == GPIO_PIN_RESET) {
            if (snake.dir != LEFT) snake.dir = RIGHT;
        }
    }
}

// 游戏初始化 (供 FreeRTOS 画面任务调用)
void game_init() {
    lcd_clear(BLACK);
    snake.len = 3;
    snake.dir = RIGHT;
    snake.x[0] = 100; snake.y[0] = 100;
    snake.x[1] = 90;  snake.y[1] = 100;
    snake.x[2] = 80;  snake.y[2] = 100;

    is_dead = 0;
    spawn_food();
}

// 游戏物理帧更新 (供 FreeRTOS 画面任务调用)
void game_update() {
    if (is_dead) return;

    draw_block(snake.x[snake.len - 1], snake.y[snake.len - 1], BLACK);

    for (int i = snake.len - 1; i > 0; i--) {
        snake.x[i] = snake.x[i - 1];
        snake.y[i] = snake.y[i - 1];
    }

    if (snake.dir == UP)    snake.y[0] -= BLOCK_SIZE;
    if (snake.dir == DOWN)  snake.y[0] += BLOCK_SIZE;
    if (snake.dir == LEFT)  snake.x[0] -= BLOCK_SIZE;
    if (snake.dir == RIGHT) snake.x[0] += BLOCK_SIZE;

    if (snake.x[0] < 0 || snake.x[0] >= MAX_X || snake.y[0] < 0 || snake.y[0] >= MAX_Y) {
        is_dead = 1;
    }

    for (int i = 1; i < snake.len; i++) {
        if (snake.x[0] == snake.x[i] && snake.y[0] == snake.y[i]) {
            is_dead = 1;
        }
    }

    if (is_dead) {
        beep_cmd = 2; // 发射死亡音效指令
        lcd_show_string(60, 130, 200, 16, 16, "GAME OVER!", RED);
        lcd_show_string(30, 160, 200, 16, 16, "Press WK_UP to Retry", WHITE);
        return;
    }

    if (snake.x[0] == food_x && snake.y[0] == food_y) {
        if (snake.len < MAX_LEN) snake.len++;
        spawn_food();
        beep_cmd = 1; // 发射吃苹果音效指令
    }

    draw_block(snake.x[0], snake.y[0], RED);
    draw_block(snake.x[1], snake.y[1], WHITE);
}

int get_snake_len(void) {
    return snake.len;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* MCU Configuration--------------------------------------------------------*/
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_FSMC_Init();
  MX_TIM3_Init();
  MX_TIM1_Init();

    /* USER CODE BEGIN 2 */
    // 💡 把它们放回这里！在操作系统接管 CPU 之前，把底层外设全弄好！
    delay_init(168);
    lcd_init();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_SET);

    srand(HAL_GetTick()); // 设置随机数种子

    // ⚠️ 警告：为了防止红外引脚悬空产生干扰，红外这两句务必先保持注释！
    // 等贪吃蛇彻底能动了，我们再去动红外！
    HAL_TIM_IC_Start_IT(&htim1, TIM_CHANNEL_1);
    __HAL_TIM_ENABLE_IT(&htim1, TIM_IT_UPDATE);
    /* USER CODE END 2 */

    /* Call init function for freertos objects (in cmsis_os2.c) */
    MX_FREERTOS_Init();

    /* Start scheduler */
    osKernelStart();

  /* We should never get here as control is now taken by the scheduler */
  while (1)
  {
  }
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
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
/* USER CODE END 4 */

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */
  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM4)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */
    // 红外接收超时复位逻辑
    if (htim->Instance == TIM1) {
        extern uint8_t IR_State;
        IR_State = 0;
        __HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_1, TIM_INPUTCHANNELPOLARITY_RISING);
    }
  /* USER CODE END Callback 1 */
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
}
#endif /* USE_FULL_ASSERT */