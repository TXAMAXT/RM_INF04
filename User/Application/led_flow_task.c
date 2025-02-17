/**
  ****************************(C) COPYRIGHT 2019 DJI****************************
  * @file       led_trigger_task.c/h
  * @brief      led RGB show.led RGB灯效。
  * @note       
  * @history
  *  Version    Date            Author          Modification
  *  V1.0.0     Nov-11-2019     RM              1. rgb led
  *
  @verbatim
  ==============================================================================

  ==============================================================================
  @endverbatim
  ****************************(C) COPYRIGHT 2019 DJI****************************
  */
#include "led_flow_task.h"
#include "bsp_led.h"
#include "cmsis_os.h"
#include "main.h"
#include "DWT.h"


#define RGB_FLOW_COLOR_CHANGE_TIME  1000
#define RGB_FLOW_COLOR_LENGHT   6

#if INCLUDE_uxTaskGetStackHighWaterMark
uint32_t led_RGB_flow_task_stack;
#endif

//blue-> green(dark)-> red -> blue(dark) -> green(dark) -> red(dark) -> blue
//蓝 -> 绿(灭) -> 红 -> 蓝(灭) -> 绿 -> 红(灭) -> 蓝 
uint32_t RGB_flow_color[
        RGB_FLOW_COLOR_LENGHT + 1] = {0xFF0000FF, 0x0000FF00, 0xFFFF0000, 0x000000FF, 0xFF00FF00, 0x00FF0000,
                                      0xFF0000FF};

/**
  * @brief          led rgb task
  * @param[in]      pvParameters: NULL
  * @retval         none
  */
/**
  * @brief          led RGB任务
  * @param[in]      pvParameters: NULL
  * @retval         none
  */
void led_RGB_flow_task(void const *argument) {
    uint16_t i, j;
    float32_t delta_alpha, delta_red, delta_green, delta_blue;
    float32_t alpha, red, green, blue;
    uint32_t aRGB;
    TickType_t LoopStartTime;
    while (1) {
        DWT_get_time_interval_us(&global_task_time.tim_led_RGB_flow_task);
        LoopStartTime = xTaskGetTickCount();
        for (i = 0; i < RGB_FLOW_COLOR_LENGHT; i++) {
            alpha = (RGB_flow_color[i] & 0xFF000000) >> 24;
            red = ((RGB_flow_color[i] & 0x00FF0000) >> 16);
            green = ((RGB_flow_color[i] & 0x0000FF00) >> 8);
            blue = ((RGB_flow_color[i] & 0x000000FF) >> 0);

            delta_alpha = (float32_t) ((RGB_flow_color[i + 1] & 0xFF000000) >> 24) -
                          (float32_t) ((RGB_flow_color[i] & 0xFF000000) >> 24);
            delta_red = (float32_t) ((RGB_flow_color[i + 1] & 0x00FF0000) >> 16) -
                        (float32_t) ((RGB_flow_color[i] & 0x00FF0000) >> 16);
            delta_green =
                    (float32_t) ((RGB_flow_color[i + 1] & 0x0000FF00) >> 8) - (float32_t) ((RGB_flow_color[i] & 0x0000FF00) >> 8);
            delta_blue =
                    (float32_t) ((RGB_flow_color[i + 1] & 0x000000FF) >> 0) - (float32_t) ((RGB_flow_color[i] & 0x000000FF) >> 0);

            delta_alpha /= RGB_FLOW_COLOR_CHANGE_TIME;
            delta_red /= RGB_FLOW_COLOR_CHANGE_TIME;
            delta_green /= RGB_FLOW_COLOR_CHANGE_TIME;
            delta_blue /= RGB_FLOW_COLOR_CHANGE_TIME;
            for (j = 0; j < RGB_FLOW_COLOR_CHANGE_TIME; j++) {
                alpha += delta_alpha;
                red += delta_red;
                green += delta_green;
                blue += delta_blue;

                aRGB = ((uint32_t) (alpha)) << 24 | ((uint32_t) (red)) << 16 | ((uint32_t) (green)) << 8 |
                       ((uint32_t) (blue)) << 0;
                aRGB_led_show(aRGB);
                vTaskDelay(pdMS_TO_TICKS(5));
            }
        }
#if INCLUDE_uxTaskGetStackHighWaterMark
        led_RGB_flow_task_stack = uxTaskGetStackHighWaterMark(NULL);
#endif
        vTaskDelayUntil(&LoopStartTime, pdMS_TO_TICKS(100));
    }
}

/**
  * @brief          获取led_RGB_flow_task栈大小
  * @param[in]      none
  * @retval         led_RGB_flow_task_stack:任务堆栈大小
  */
uint32_t get_stack_of_led_RGB_flow_task(void) {
    return led_RGB_flow_task_stack;
}


