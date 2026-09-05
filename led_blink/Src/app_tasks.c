#include "app_tasks.h"
#include "main.h"
#include "usart.h"
#include "cmsis_os.h"     /* osDelay 的声明在这里（CMSIS_V2 工程），不加会报 implicit declaration */
#include <stdio.h>

/* ---------- 全局变量 ---------- */
static TaskHandle_t s_taskA = NULL;
static TaskHandle_t s_taskB = NULL;
static TaskHandle_t s_taskC = NULL;
static QueueHandle_t s_queue = NULL;

/* ---------- 任务 A：生产者 ---------- */
static void TaskA_Producer(void *argument)
{
    uint32_t num = 0;

    for (;;)
    {
        num++;
        /* 往队尾放数据，第三个参数是"队列满时最多等多久"，portMAX_DELAY = 一直等 */
        if (xQueueSend(s_queue, &num, portMAX_DELAY) == pdPASS)
        {
            printf("[A] send %lu\r\n", num);
        }
        osDelay(1000);        /* 1 秒一次 */
    }
}

/* ---------- 任务 B：消费者 ---------- */
static void TaskB_Consumer(void *argument)
{
    uint32_t recv = 0;

    for (;;)
    {
        /* 队列空时，任务 B 会在这里"阻塞"，完全不占 CPU（任务 C 照跑） */
        if (xQueueReceive(s_queue, &recv, portMAX_DELAY) == pdPASS)
        {
            printf("[B] recv %lu\r\n", recv);
        }
    }
}

/* ---------- 任务 C：心跳灯 ---------- */
static void TaskC_Heartbeat(void *argument)
{
    for (;;)
    {
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_2);   /* 板载 LED = PB2（天空星）。别用 PA7：那是板载 W25Q128（SPI Flash）的引脚 */
        osDelay(500);
    }
}

/* ---------- 创建任务 ---------- */
void App_Tasks_Create(void)
{
    s_queue = xQueueCreate(5, sizeof(uint32_t));   /* 队列长度 5，元素 4 字节 */

    xTaskCreate(TaskA_Producer, "TaskA", 256, NULL, 3, &s_taskA);
    xTaskCreate(TaskB_Consumer, "TaskB", 256, NULL, 4, &s_taskB);
    xTaskCreate(TaskC_Heartbeat,"TaskC", 128, NULL, 2, &s_taskC);
}
