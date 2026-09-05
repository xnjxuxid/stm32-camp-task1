/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : CAN 瀵圭锛堜俊鍙锋簮锛夆?斺?? STM32F103C8T6
  *
  *  鍔熻兘锛氭瘡 500 ms 鍙戜竴甯? CAN 鎶ユ枃锛屾瘡 5 绉掓崲涓?涓懠鍚稿懆鏈?
  *        锛?2000 ms -> 1000 ms -> 400 ms 寰幆锛夛紝鐢ㄤ簬楠屾敹浠诲姟涓?鐨?
  *        "CAN 鎺ユ敹涓柇 -> 闃熷垪 -> 浠诲姟閫氱煡 -> 淇敼鍛煎惛鐏鐜?"銆?
  *
  *  甯ф牸寮忥紙涓庡ぉ绌烘槦浠诲姟涓?宸ョ▼涓?鑷达級锛?
  *        [0]=0xAA澶村抚 [1]=CMD [2..3]=鏁版嵁(澶хu16)
  *        [4]=鏍￠獙鍜?(CMD+DH+DL) [5..6]=0x00 [7]=0x55灏惧抚
  *
  *  浣跨敤鏂规硶锛?
  *    1. CubeMX 鎵撳紑 can_sender.ioc -> GENERATE CODE
  *    2. 鐢ㄦ湰鏂囦欢瑕嗙洊鐢熸垚鍑烘潵鐨? Src/main.c
  *    3. Keil 缂栬瘧锛圡icroLIB 瑕佸嬀涓婏級-> 涓嬭浇
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "can.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
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

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* printf 閲嶅畾鍚戝埌 USART1锛坒putc 鐗堟湰锛孠eil 闇?鍕鹃?? MicroLIB锛? */
#ifdef __GNUC__
  #define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
  #define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif

PUTCHAR_PROTOTYPE
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 1000);
    return ch;
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

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_CAN_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  CAN_TxHeaderTypeDef txHeader;
  uint8_t             txData[8];
  uint32_t            txMailbox;
  const uint16_t      periodTable[3] = {2000u, 1000u, 400u};  /* 渚濇鍒囨崲鐨勫懠鍚稿懆鏈?(ms) */
  uint8_t             idx  = 0;
  uint32_t            seq  = 0;

  if (HAL_CAN_Start(&hcan) != HAL_OK)
  {
      printf("CAN start FAILED!\r\n");
      Error_Handler();
  }

  txHeader.StdId              = 0x123;        /* 鏍囧噯 ID锛屽ぉ绌烘槦绔笉杩囨护锛屼换鎰? ID 閮芥敹 */
  txHeader.ExtId              = 0x00;
  txHeader.IDE                = CAN_ID_STD;
  txHeader.RTR                = CAN_RTR_DATA;
  txHeader.DLC                = 8;
  txHeader.TransmitGlobalTime = DISABLE;

  printf("\r\n=== CAN sender (F103C8T6) 500kbps ===\r\n");
  printf("Send a frame every 500ms, change period every 5s\r\n");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint16_t          period;
    HAL_StatusTypeDef st;
    uint32_t          t0;
    uint32_t          esr;
    uint8_t           boff;
    uint8_t           lec;
    uint8_t           tec;

    period = periodTable[idx];

    txData[0] = 0xAA;                                  /* 澶村抚 */
    txData[1] = 0x01;                                  /* CMD: 淇敼鍛煎惛鍛ㄦ湡 */
    txData[2] = (uint8_t)(period >> 8);                /* 鏁版嵁楂樺瓧鑺? */
    txData[3] = (uint8_t)(period & 0xFFU);             /* 鏁版嵁浣庡瓧鑺? */
    txData[4] = (uint8_t)(txData[1] + txData[2] + txData[3]);  /* 鏍￠獙鍜? */
    txData[5] = 0x00;
    txData[6] = 0x00;
    txData[7] = 0x55;                                  /* 灏惧抚 */

    st = HAL_CAN_AddTxMessage(&hcan, &txHeader, txData, &txMailbox);

    /* 等这一帧发完（最多 50 ms）。
     * 注意：AutoRetransmission = DISABLE，失败不会重传，所以邮箱很快释放，
     *       HAL_CAN_AddTxMessage 返回 OK 只代表"报文进了邮箱"，
     *       **不代表真的发到了总线上** —— 必须再查错误状态寄存器。 */
    t0 = HAL_GetTick();
    while (HAL_CAN_IsTxMessagePending(&hcan, txMailbox))
    {
        if ((HAL_GetTick() - t0) > 50U)
        {
            break;
        }
    }

    /* CAN 错误状态寄存器 ESR：
     *   bit2      = BOFF (总线关闭)
     *   bit[6:4]  = LEC  最后一次错误码，3 = 应答错误(没人接)
     *   bit[23:16]= TEC  发送错误计数，>255 进入 Bus-Off */
    esr  = hcan.Instance->ESR;
    boff = (esr & (1UL << 2)) ? 1U : 0U;
    lec  = (uint8_t)((esr >> 4) & 0x07U);
    tec  = (uint8_t)((esr >> 16) & 0xFFU);

    ++seq;

    if (boff)
    {
        printf("[%lu] BUS-OFF (TEC=%u) -> 总线上没有节点应答，正在重启 CAN...\r\n", seq, tec);
        HAL_CAN_Stop(&hcan);
        HAL_CAN_Start(&hcan);        /* 重新启动即请求退出 Bus-Off */
    }
    else if (lec == 3U)              /* No Acknowledge error */
    {
        printf("[%lu] TX -> ACK ERROR (TEC=%u) 帧已发出但没人应答："
               "需要 2 个节点 + 两端 120Ω + 共地\r\n", seq, tec);
    }
    else if (st == HAL_OK)
    {
        printf("[%lu] TX OK : id=0x123  %02X %02X %02X %02X %02X %02X %02X %02X"
               "  (breath period = %u ms)\r\n",
               seq,
               txData[0], txData[1], txData[2], txData[3],
               txData[4], txData[5], txData[6], txData[7],
               period);
    }
    else
    {
        printf("[%lu] TX 入队失败 (err=%d)\r\n", seq, st);
    }

    /* 姣? 10 甯э紙绾? 5 绉掞級鎹竴涓懆鏈燂紝鏂逛究瑙傚療澶╃┖鏄熷懠鍚哥伅棰戠巼鍙樺寲 */
    if ((seq % 10U) == 0U)
    {
        idx = (idx + 1U) % 3U;
    }

    HAL_Delay(500);
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
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

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
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

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
