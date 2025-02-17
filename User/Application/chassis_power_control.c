/**
  ****************************(C) COPYRIGHT 2019 DJI****************************
  * @file       chassis_power_control.c/h
  * @brief      chassis power control.底盘功率控制
  * @note       this is only controling 80 w power, mainly limit motor current set.
  *             if power limit is 40w, reduce the value JUDGE_TOTAL_CURRENT_LIMIT 
  *             and POWER_CURRENT_LIMIT, and chassis max speed (include max_vx_speed, min_vx_speed)
  *             只控制80w功率，主要通过控制电机电流设定值,如果限制功率是40w，减少
  *             JUDGE_TOTAL_CURRENT_LIMIT和POWER_CURRENT_LIMIT的值，还有底盘最大速度
  *             (包括max_vx_speed, min_vx_speed)
  * @history
  *  Version    Date            Author          Modification
  *  V1.0.0     Nov-11-2019     RM              1. add chassis power control
  *
  @verbatim
  ==============================================================================

  ==============================================================================
  @endverbatim
  ****************************(C) COPYRIGHT 2019 DJI****************************
  */
#include "chassis_power_control.h"
#include "referee_task.h"
#include "arm_math.h"
#include "detect_task.h"
#include "SEGGER_RTT.h"
/**
  * @brief          limit the power, mainly limit motor current
  * @param[in]      chassis_power_control: chassis data 
  * @retval         none
  */
/**
  * @brief          限制功率，主要限制电机电流
  * @param[in]      chassis_power_control: 底盘数据
  * @retval         none
  */
void chassis_power_control(chassis_move_t *chassis_power_control) {
    float32_t chassis_power = 0.0f;
    float32_t chassis_power_buffer = 0.0f;
    float32_t total_current_limit = 0.0f;
    float32_t total_current = 0.0f;
    uint8_t robot_id = get_robot_id();
    if (toe_is_error(SUPER_CAPACITANCE_TOE)) {
        chassis_power_control->soft_power_limit = chassis_power_control->power_limit;
    } else{
//        SEGGER_RTT_printf(0,"%d",chassis_power_control->soft_power_limit);
        if(!super_capacitance_enable_flag){
            chassis_power_control->soft_power_limit = chassis_power_control->power_limit;
        }
    }
    if (toe_is_error(REFEREE_RX_TOE)) {
        total_current_limit = NO_JUDGE_TOTAL_CURRENT_LIMIT;
    } else if (robot_id == 2 || robot_id == 102 || robot_id == 0) {
        total_current_limit = NO_JUDGE_TOTAL_CURRENT_LIMIT;
    } else {
//        SEGGER_RTT_printf(0,"%d\r\n",chassis_power_control->soft_power_limit);
        get_chassis_power_and_buffer(&chassis_power, &chassis_power_buffer);
        // power > 80w and buffer < 60j, because buffer < 60 means power has been more than 80w
        //功率超过80w 和缓冲能量小于60j,因为缓冲能量小于60意味着功率超过80w
        if (chassis_power_buffer < WARNING_POWER_BUFF) {
            float32_t power_scale;
            if (chassis_power_buffer > 6.0f) {
                //scale down WARNING_POWER_BUFF
                //缩小WARNING_POWER_BUFF
                power_scale = chassis_power_buffer / WARNING_POWER_BUFF;
            } else {
                //only left 10% of WARNING_POWER_BUFF
                power_scale = 6.0f / WARNING_POWER_BUFF;
            }
            //scale down
            //缩小
            total_current_limit = BUFFER_TOTAL_CURRENT_LIMIT * power_scale;
        } else {
            //power > WARNING_POWER
            //功率大于WARNING_POWER
            if (chassis_power > (chassis_power_control->soft_power_limit - WARNING_POWER)) {
                float32_t power_scale;
                //power < 60w
                //功率小于60w
                if (chassis_power < chassis_power_control->soft_power_limit) {
                    //scale down
                    //缩小
                    power_scale = (chassis_power_control->soft_power_limit - chassis_power) /
                                  (chassis_power_control->soft_power_limit -
                                   (chassis_power_control->soft_power_limit - WARNING_POWER));

                } else {
                    //power > 80w
                    //功率大于80w
                    power_scale = 0.0f;
                }

                total_current_limit = BUFFER_TOTAL_CURRENT_LIMIT + POWER_TOTAL_CURRENT_LIMIT * power_scale;
            } else {
                //power < WARNING_POWER
                //功率小于WARNING_POWER
                total_current_limit = BUFFER_TOTAL_CURRENT_LIMIT + POWER_TOTAL_CURRENT_LIMIT;
            }
        }
    }


    total_current = 0.0f;
    //calculate the original motor current set
    //计算原本电机电流设定
    for (uint8_t i = 0; i < 4; i++) {
        total_current += fabsf(chassis_power_control->motor_speed_pid[i].out);
    }


    if (total_current > total_current_limit) {
        float32_t current_scale = total_current_limit / total_current;
        chassis_power_control->motor_speed_pid[0].out *= current_scale;
        chassis_power_control->motor_speed_pid[1].out *= current_scale;
        chassis_power_control->motor_speed_pid[2].out *= current_scale;
        chassis_power_control->motor_speed_pid[3].out *= current_scale;
    }
}
