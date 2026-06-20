/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : 贪吃蛇 FreeRTOS 任务调度器 (三线程并行架构)
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
extern void game_init(void);
extern void game_update(void);
extern void key_scan(void);
extern int get_snake_len(void);
extern int beep_cmd;

// 💡 声明外部的屏幕和延时初始化函数
extern void delay_init(uint8_t SYSCLK);
extern void lcd_init(void);
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
/* USER CODE BEGIN Variables */
/* USER CODE END Variables */
osThreadId GameTaskHandle;
osThreadId KeyTaskHandle;
osThreadId BuzzerTaskHandle;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
/* USER CODE END FunctionPrototypes */

void StartGameTask(void const * argument);
void StartKeyTask(void const * argument);
void StartBuzzerTask(void const * argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* Create the thread(s) */
  /* definition and creation of GameTask */
  osThreadDef(GameTask, StartGameTask, osPriorityNormal, 0, 256); // 建议堆栈给256
  GameTaskHandle = osThreadCreate(osThread(GameTask), NULL);

  /* definition and creation of KeyTask */
  osThreadDef(KeyTask, StartKeyTask, osPriorityAboveNormal, 0, 128);
  KeyTaskHandle = osThreadCreate(osThread(KeyTask), NULL);

  /* definition and creation of BuzzerTask */
  osThreadDef(BuzzerTask, StartBuzzerTask, osPriorityNormal, 0, 128);
  BuzzerTaskHandle = osThreadCreate(osThread(BuzzerTask), NULL);
}

/* USER CODE BEGIN Header_StartGameTask */
/**
  * @brief  大厨一号：游戏画面刷新任务
  */
/* USER CODE END Header_StartGameTask */
void StartGameTask(void const * argument)
{
  /* USER CODE BEGIN StartGameTask */

  game_init(); // 任务开始前，先初始化第一局游戏画面

  /* Infinite loop */
  for(;;)
  {
    game_update(); // 算一帧，画一帧

    // 动态难度核心算法
    int current_delay = 400 - (get_snake_len() - 3) * 10;
    if (current_delay < 60) {
      current_delay = 60;
    }
    osDelay(current_delay);
  }
  /* USER CODE END StartGameTask */
}

/* USER CODE BEGIN Header_StartKeyTask */
/**
* @brief 大厨二号：按键极速扫描任务
*/
/* USER CODE END Header_StartKeyTask */
void StartKeyTask(void const * argument)
{
  /* USER CODE BEGIN StartKeyTask */
  /* Infinite loop */
  for(;;)
  {
    key_scan();
    osDelay(20);
  }
  /* USER CODE END StartKeyTask */
}

/* USER CODE BEGIN Header_StartBuzzerTask */
/**
* @brief 大厨三号：专门负责控制蜂鸣器发声的任务
*/
/* USER CODE END Header_StartBuzzerTask */
void StartBuzzerTask(void const * argument)
{
  /* USER CODE BEGIN StartBuzzerTask */
  /* Infinite loop */
  for(;;)
  {
    if (beep_cmd == 1) {
        HAL_GPIO_WritePin(GPIOF, GPIO_PIN_8, GPIO_PIN_SET);
        osDelay(50);
        HAL_GPIO_WritePin(GPIOF, GPIO_PIN_8, GPIO_PIN_RESET);
        beep_cmd = 0;
    }
    else if (beep_cmd == 2) {
        HAL_GPIO_WritePin(GPIOF, GPIO_PIN_8, GPIO_PIN_SET);
        osDelay(150);
        HAL_GPIO_WritePin(GPIOF, GPIO_PIN_8, GPIO_PIN_RESET);

        osDelay(100);

        HAL_GPIO_WritePin(GPIOF, GPIO_PIN_8, GPIO_PIN_SET);
        osDelay(300);
        HAL_GPIO_WritePin(GPIOF, GPIO_PIN_8, GPIO_PIN_RESET);
        beep_cmd = 0;
    }
    osDelay(20);
  }
  /* USER CODE END StartBuzzerTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
/* USER CODE END Application */

/* USER CODE BEGIN GET_IDLE_TASK_MEMORY */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
  *ppxIdleTaskStackBuffer = &xIdleStack[0];
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}
/* USER CODE END GET_IDLE_TASK_MEMORY */