#ifndef __APP_TASKS_H
#define __APP_TASKS_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* ---------------- 三个任务的栈大小（单位：字 = 4 字节） ----------------
 * 说明：任务里用了 printf / snprintf 这类吃栈的函数，至少给 256。
 * --------------------------------------------------------------------*/
#define TASK_CAN_STACK          (256u)
#define TASK_BREATH_STACK       (128u)
#define TASK_UART_STACK         (256u)

/* ---------------- 优先级（数字越大优先级越高） ---------------- */
#define TASK_CAN_PRIO           (4)
#define TASK_UART_PRIO          (3)
#define TASK_BREATH_PRIO        (3)

/* 创建任务一所需的全部任务与模块（在 freertos.c 的 MX_FREERTOS_Init 里调用） */
void App_Tasks_Create(void);

#endif /* __APP_TASKS_H */
