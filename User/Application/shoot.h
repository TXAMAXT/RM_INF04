/**
  ****************************(C) COPYRIGHT 2019 DJI****************************
  * @file       shoot.c/h
  * @brief      射击功能。
  * @note       
  * @history
  *  Version    Date            Author          Modification
  *  V1.0.0     Dec-26-2018     RM              1. 完成
  *
  @verbatim
  ==============================================================================

  ==============================================================================
  @endverbatim
  ****************************(C) COPYRIGHT 2019 DJI****************************
  */

#ifndef SHOOT_H
#define SHOOT_H

#include "struct_typedef.h"

#include "CAN_receive.h"
#include "gimbal_task.h"
#include "remote_control.h"
#include "user_lib.h"



//射击发射开关通道数据
#define RADIO_CONTROL_SWITCH_R       0
#define RADIO_CONTROL_SWITCH_L       1
//云台模式使用的开关通道

#define SHOOT_CONTROL_TIME          GIMBAL_CONTROL_TIME

#define SHOOT_FRIC_PWM_ADD_VALUE    20

//射击摩擦轮激光打开 关闭
#define SHOOT_KEYBOARD           KEY_PRESSED_OFFSET_B

//射击完成后 子弹弹出去后，判断时间，以防误触发
#define SHOOT_DONE_KEY_OFF_TIME     15
//鼠标长按判断
#define PRESS_LONG_TIME             400
//遥控器射击开关打下档一段时间后 连续发射子弹 用于清单
#define RC_S_LONG_TIME              2000
//摩擦轮高速 加速 时间
#define UP_ADD_TIME                 80
//电机反馈码盘值范围
#define HALF_ECD_RANGE              4096
#define ECD_RANGE                   8191
//电机rmp 变化成 旋转速度的比例
#define MOTOR_RPM_TO_SPEED          0.00290888208665721596153948461415f
#define MOTOR_ECD_TO_ANGLE          0.000021305288720633905968306772076277f
#define FULL_COUNT                  18
//拨弹速度
#define CONTINUE_TRIGGER_SPEED      6.0f
#define CONTINUE_TRIGGER_SPEED_MAX      10.0f

#define KEY_OFF_JUGUE_TIME          500
#define SWITCH_TRIGGER_ON           0
#define SWITCH_TRIGGER_OFF          1

//卡单时间 以及反转时间
#define BLOCK_TRIGGER_SPEED         0.7f
#define BLOCK_TIME                  3500
#define REVERSE_TIME                10
#define REVERSE_SPEED_LIMIT         13.0f

#define PI_FOUR                     0.78539816339744830961566084581988f
#define PI_TEN                      0.314f

//拨弹轮电机PID
#define TRIGGER_SPEED_PID_KP        750.0f
#define TRIGGER_SPEED_PID_KI        1.2f
#define TRIGGER_SPEED_PID_KD        0.0f

#define TRIGGER_ANGLE_PID_KP        30.0f
#define TRIGGER_ANGLE_PID_KI        0.00f
#define TRIGGER_ANGLE_PID_KD        250.0f

#define TRIGGER_BULLET_PID_MAX_OUT  10000.0f
#define TRIGGER_BULLET_PID_MAX_IOUT 5000.0f

#define TRIGGER_READY_PID_MAX_OUT   16000.0f
#define TRIGGER_READY_PID_MAX_IOUT  8000.0f

#define SHOOT_SPEED_30_MS_TO_FRIC 780.0f
#define SHOOT_SPEED_18_MS_TO_FRIC 550.0f //1.7m 无下坠
#define SHOOT_SPEED_15_MS_TO_FRIC 500.0f //2.7m 无下坠

#define TRIGGER_SPEED_10 2.0f
#define TRIGGER_SPEED_15 2.5f
#define TRIGGER_SPEED_25 3.5f
#define TRIGGER_SPEED_35 4.5f
#define TRIGGER_SPEED_40 5.0f
#define TRIGGER_SPEED_60 7.0f
#define TRIGGER_SPEED_80 9.0f



#define SHOOT_HEAT_REMAIN_VALUE     80

typedef enum {
    SHOOT_STOP = 0,
    SHOOT_PID_AUTO_TUNE,
    SHOOT_START,
    SHOOT_READY,
    SHOOT_BULLET,
    SHOOT_CONTINUE_BULLET,
    SHOOT_DONE,
} shoot_mode_e;


typedef struct {
    shoot_heat_cooling_rate_e shoot_cooling_rate_referee_set;
    shoot_speed_e shoot_speed_referee_set;
    shoot_state_e shoot_fric_state;
    shoot_mode_e shoot_mode;
    const volatile RC_ctrl_t *shoot_rc;
    const volatile pid_auto_tune_t *pid_auto_tune_data_point;
    const motor_measure_t *shoot_motor_measure;

    const motor_measure_t *fric1_motor_measure;
    const motor_measure_t *fric2_motor_measure;

    ramp_function_source_t fric1_ramp;
    uint16_t fric_pwm1;
    ramp_function_source_t fric2_ramp;
    uint16_t fric_pwm2;
    pid_type_def trigger_motor_speed_pid;
    pid_type_def trigger_motor_angle_pid;
    extKalman_t Trigger_Motor_Current_Kalman_Filter;

    pid_type_def fric1_motor_pid;
    pid_type_def fric2_motor_pid;

    float32_t fric_all_speed;
    float32_t fric1_speed_set;
    float32_t fric2_speed_set;
    float32_t fric1_speed;
    float32_t fric2_speed;

    float32_t trigger_speed_set;
    float32_t speed;
    float32_t speed_set;
    float32_t angle;
    float32_t angle_set;
    float32_t current_set;
    int16_t given_current;
    int8_t ecd_count;

    bool_t press_l;
//    bool_t press_r;
    bool_t last_press_l;
//    bool_t last_press_r;
    uint16_t press_l_time;
//    uint16_t press_r_time;
    uint16_t rc_s_time;

    uint16_t block_time;
    uint16_t reverse_time;
    bool_t move_flag;

    bool_t key;
    uint8_t key_time;

    uint16_t heat_limit;
    uint16_t heat;

    int16_t pwm;
} shoot_control_t;

//由于射击和云台使用同一个can的id故也射击任务在云台任务中执行
extern void shoot_init(void);

extern int16_t shoot_control_loop(void);

extern shoot_control_t shoot_control;

#endif
