/**
  ****************************(C) COPYRIGHT 2019 DJI****************************
  * @file       gimbal_task.c/h
  * @brief      gimbal control task, because use the euler angle calculate by
  *             gyro sensor, range (-pi,pi), angle set-point must be in this 
  *             range.gimbal has two control mode, gyro mode and enconde mode
  *             gyro mode: use euler angle to control, encond mode: use enconde
  *             angle to control. and has some special mode:cali mode, motionless
  *             mode.
  *             完成云台控制任务，由于云台使用陀螺仪解算出的角度，其范围在（-pi,pi）
  *             故而设置目标角度均为范围，存在许多对角度计算的函数。云台主要分为2种
  *             状态，陀螺仪控制状态是利用板载陀螺仪解算的姿态角进行控制，编码器控制
  *             状态是通过电机反馈的编码值控制的校准，此外还有校准状态，停止状态等。
  * @note       
  * @history
  *  Version    Date            Author          Modification
  *  V1.0.0     Dec-26-2018     RM              1. done
  *  V1.1.0     Nov-11-2019     RM              1. add some annotation
  *
  @verbatim
  ==============================================================================
    add a gimbal behaviour mode
    1. in gimbal_behaviour.h , add a new behaviour name in gimbal_behaviour_e
    erum
    {  
        ...
        ...
        GIMBAL_XXX_XXX, // new add
    }gimbal_behaviour_e,
    2. implement new function. gimbal_xxx_xxx_control(float32_t *yaw, float32_t *pitch, gimbal_control_t *gimbal_control_set);
        "yaw, pitch" param is gimbal movement contorl input. 
        first param: 'yaw' usually means  yaw axis move,usaully means increment angle.
            positive value means counterclockwise move, negative value means clockwise move.
        second param: 'pitch' usually means pitch axis move,usaully means increment angle.
            positive value means counterclockwise move, negative value means clockwise move.

        in this new function, you can assign set-point to "yaw" and "pitch",as your wish
    3.  in "gimbal_behavour_set" function, add new logical judgement to assign GIMBAL_XXX_XXX to  "gimbal_behaviour" variable,
        and in the last of the "gimbal_behaviour_mode_set" function, add "else if(gimbal_behaviour == GIMBAL_XXX_XXX)" 
        choose a gimbal control mode.
        four mode:
        GIMBAL_MOTOR_RAW : will use 'yaw' and 'pitch' as motor current set,  derectly sent to can bus.
        GIMBAL_MOTOR_ENCONDE : 'yaw' and 'pitch' are angle increment,  control enconde relative angle.
        GIMBAL_MOTOR_GYRO : 'yaw' and 'pitch' are angle increment,  control gyro absolute angle.
    4. in the last of "gimbal_behaviour_control_set" function, add
        else if(gimbal_behaviour == GIMBAL_XXX_XXX)
        {
            gimbal_xxx_xxx_control(&rc_add_yaw, &rc_add_pit, gimbal_control_set);
        }

        
    如果要添加一个新的行为模式
    1.首先，在gimbal_behaviour.h文件中， 添加一个新行为名字在 gimbal_behaviour_e
    erum
    {  
        ...
        ...
        GIMBAL_XXX_XXX, // 新添加的
    }gimbal_behaviour_e,

    2. 实现一个新的函数 gimbal_xxx_xxx_control(float32_t *yaw, float32_t *pitch, gimbal_control_t *gimbal_control_set);
        "yaw, pitch" 参数是云台运动控制输入量
        第一个参数: 'yaw' 通常控制yaw轴移动,通常是角度增量,正值是逆时针运动,负值是顺时针
        第二个参数: 'pitch' 通常控制pitch轴移动,通常是角度增量,正值是逆时针运动,负值是顺时针
        在这个新的函数, 你能给 "yaw"和"pitch"赋值想要的参数
    3.  在"gimbal_behavour_set"这个函数中，添加新的逻辑判断，给gimbal_behaviour赋值成GIMBAL_XXX_XXX
        在gimbal_behaviour_mode_set函数最后，添加"else if(gimbal_behaviour == GIMBAL_XXX_XXX)" ,然后选择一种云台控制模式
        3种:
        GIMBAL_MOTOR_RAW : 使用'yaw' and 'pitch' 作为电机电流设定值,直接发送到CAN总线上.
        GIMBAL_MOTOR_ENCONDE : 'yaw' and 'pitch' 是角度增量,  控制编码相对角度.
        GIMBAL_MOTOR_GYRO : 'yaw' and 'pitch' 是角度增量,  控制陀螺仪绝对角度.
    4.  在"gimbal_behaviour_control_set" 函数的最后，添加
        else if(gimbal_behaviour == GIMBAL_XXX_XXX)
        {
            gimbal_xxx_xxx_control(&rc_add_yaw, &rc_add_pit, gimbal_control_set);
        }
  ==============================================================================
  @endverbatim
  ****************************(C) COPYRIGHT 2019 DJI****************************
  */

#include "gimbal_behaviour.h"
#include "arm_math.h"
#include "bsp_buzzer.h"
#include "detect_task.h"
#include "user_lib.h"
#include "USER_Filter.h"
#include "vision_task.h"
#include "math.h"
#include "global_control_define.h"
#include "ahrs_ukf.h"
#include "calibrate_task.h"
#include "gimbal_task.h"
#include "chassis_behaviour.h"
#include "pid_auto_tune_task.h"
#include "print_task.h"
#include "SEGGER_RTT.h"

//when gimbal is in calibrating, set buzzer frequency and strenght
//当云台在校准, 设置蜂鸣器频率和强度
#define gimbal_warn_buzzer_on() buzzer_on(31, VOLUME(50))
#define gimbal_warn_buzzer_off() buzzer_off()

#define int_abs(x) ((x) > 0 ? (x) : (-x))



/**
  * @brief          remote control dealline solve,because the value of rocker is not zero in middle place,
  * @param          input:the raw channel value 
  * @param          output: the processed channel value
  * @param          deadline
  */
/**
  * @brief          遥控器的死区判断，因为遥控器的拨杆在中位的时候，不一定为0，
  * @param          输入的遥控器值
  * @param          输出的死区处理后遥控器值
  * @param          死区值
  */
#define rc_deadband_limit(input, output, dealine)        \
    {                                                    \
        if ((input) > (dealine) || (input) < -(dealine)) \
        {                                                \
            (output) = (input);                          \
        }                                                \
        else                                             \
        {                                                \
            (output) = 0;                                \
        }                                                \
    }


/**
  * @brief          judge if gimbal reaches the limit by gyro
  * @param          gyro: rotation speed unit rad/s
  * @param          timing time, input "GIMBAL_CALI_STEP_TIME"
  * @param          record angle, unit rad
  * @param          feedback angle, unit rad
  * @param          record ecd, unit raw
  * @param          feedback ecd, unit raw
  * @param          cali step, +1 by one step
  */
/**
  * @brief          通过判断角速度来判断云台是否到达极限位置
  * @param          对应轴的角速度，单位rad/s
  * @param          计时时间，到达GIMBAL_CALI_STEP_TIME的时间后归零
  * @param          记录的角度 rad
  * @param          反馈的角度 rad
  * @param          记录的编码值 raw
  * @param          反馈的编码值 raw
  * @param          校准的步骤 完成一次 加一
  */
#define gimbal_cali_gyro_judge(gyro, cmd_time, angle_set, angle, ecd_set, ecd, step) \
    {                                                                                \
        if ((gyro) < GIMBAL_CALI_GYRO_LIMIT)                                         \
        {                                                                            \
            (cmd_time)++;                                                            \
            if ((cmd_time) > GIMBAL_CALI_STEP_TIME)                                  \
            {                                                                        \
                (cmd_time) = 0;                                                      \
                (angle_set) = (angle);                                               \
                (ecd_set) = (ecd);                                                   \
                (step)++;                                                            \
            }                                                                        \
        }                                                                            \
    }
/**
  * @brief          通过判断电机转速来判断云台是否到达极限位置
  * @param          对应轴的角速度，单位rad/s
  * @param          计时时间，到达GIMBAL_CALI_STEP_TIME的时间后归零
  * @param          记录的角度 rad
  * @param          反馈的角度 rad
  * @param          记录的编码值 raw
  * @param          反馈的编码值 raw
  * @param          校准的步骤 完成一次 加一
  */
#define gimbal_cali_motor_speed_judge(motor_speed, cmd_time, angle_set, angle, ecd_set, ecd, step) \
    {                                                                                \
        if ((motor_speed) < GIMBAL_CALI_GYRO_LIMIT)                                         \
        {                                                                            \
            (cmd_time)++;                                                            \
            if ((cmd_time) > GIMBAL_CALI_STEP_TIME)                                  \
            {                                                                        \
                (cmd_time) = 0;                                                      \
                (angle_set) = (angle);                                               \
                (ecd_set) = (ecd);                                                   \
                (step)++;                                                            \
            }                                                                        \
        }                                                                            \
    }

/**
  * @brief          gimbal behave mode set.
  * @param[in]      gimbal_mode_set: gimbal data
  * @retval         none
  */
/**
  * @brief          云台行为状态机设置.
  * @param[in]      gimbal_mode_set: 云台数据指针
  * @retval         none
  */
static void gimbal_behavour_set(gimbal_control_t *gimbal_mode_set);

/**
  * @brief          when gimbal behaviour mode is GIMBAL_ZERO_FORCE, the function is called
  *                 and gimbal control mode is raw. The raw mode means set value
  *                 will be sent to CAN bus derectly, and the function will set all zero.
  * @param[out]     yaw: yaw motor current set, it will be sent to CAN bus derectly.
  * @param[out]     pitch: pitch motor current set, it will be sent to CAN bus derectly.
  * @param[in]      gimbal_control_set: gimbal data
  * @retval         none
  */
/**
  * @brief          当云台行为模式是GIMBAL_ZERO_FORCE, 这个函数会被调用,云台控制模式是raw模式.原始模式意味着
  *                 设定值会直接发送到CAN总线上,这个函数将会设置所有为0.
  * @param[in]      yaw:发送yaw电机的原始值，会直接通过can 发送到电机
  * @param[in]      pitch:发送pitch电机的原始值，会直接通过can 发送到电机
  * @param[in]      gimbal_control_set: 云台数据指针
  * @retval         none
  */
static void gimbal_zero_force_control(float32_t *yaw, float32_t *pitch, gimbal_control_t *gimbal_control_set);

/**
  * @brief          when gimbal behaviour mode is GIMBAL_INIT, the function is called
  *                 and gimbal control mode is gyro mode. gimbal will lift the pitch axis
  *                 and rotate yaw axis.
  * @param[out]     yaw: yaw motor relative angle increment, unit rad.
  * @param[out]     pitch: pitch motor absolute angle increment, unit rad.
  * @param[in]      gimbal_control_set: gimbal data
  * @retval         none
  */
/**
  * @brief          云台初始化控制，电机是陀螺仪角度控制，云台先抬起pitch轴，后旋转yaw轴
  * @param[out]     yaw轴角度控制，为角度的增量 单位 rad
  * @param[out]     pitch轴角度控制，为角度的增量 单位 rad
  * @param[in]      云台数据指针
  * @retval         返回空
  */
static void gimbal_init_control(float32_t *yaw, float32_t *pitch, gimbal_control_t *gimbal_control_set);

/**
  * @brief          when gimbal behaviour mode is GIMBAL_CALI, the function is called
  *                 and gimbal control mode is raw mode. gimbal will lift the pitch axis, 
  *                 and then put down the pitch axis, and rotate yaw axis counterclockwise,
  *                 and rotate yaw axis clockwise.
  * @param[out]     yaw: yaw motor current set, will be sent to CAN bus decretly
  * @param[out]     pitch: pitch motor current set, will be sent to CAN bus decretly
  * @param[in]      gimbal_control_set: gimbal data
  * @retval         none
  */
/**
  * @brief          云台校准控制，电机是raw控制，云台先抬起pitch，放下pitch，在正转yaw，最后反转yaw，记录当时的角度和编码值
  * @author         RM
  * @param[out]     yaw:发送yaw电机的原始值，会直接通过can 发送到电机
  * @param[out]     pitch:发送pitch电机的原始值，会直接通过can 发送到电机
  * @param[in]      gimbal_control_set:云台数据指针
  * @retval         none
  */
static void gimbal_cali_control(float32_t *yaw, float32_t *pitch, gimbal_control_t *gimbal_control_set);

static void ist_cali_control(float32_t *yaw, float32_t *pitch, gimbal_control_t *gimbal_control_set);
/**
  * @brief          when gimbal behaviour mode is GIMBAL_ABSOLUTE_ANGLE, the function is called
  *                 and gimbal control mode is gyro mode. 
  * @param[out]     yaw: yaw axia absolute angle increment, unit rad
  * @param[out]     pitch: pitch axia absolute angle increment,unit rad
  * @param[in]      gimbal_control_set: gimbal data
  * @retval         none
  */
/**
  * @brief          云台陀螺仪控制，电机是陀螺仪角度控制，
  * @param[out]     yaw: yaw轴角度控制，为角度的增量 单位 rad
  * @param[out]     pitch:pitch轴角度控制，为角度的增量 单位 rad
  * @param[in]      gimbal_control_set:云台数据指针
  * @retval         none
  */
static void gimbal_absolute_angle_control(float32_t *yaw, float32_t *pitch, gimbal_control_t *gimbal_control_set);

/**
  * @brief          when gimbal behaviour mode is GIMBAL_RELATIVE_ANGLE, the function is called
  *                 and gimbal control mode is encode mode. 
  * @param[out]     yaw: yaw axia relative angle increment, unit rad
  * @param[out]     pitch: pitch axia relative angle increment,unit rad
  * @param[in]      gimbal_control_set: gimbal data
  * @retval         none
  */
/**
  * @brief          云台编码值控制，电机是相对角度控制，
  * @param[in]      yaw: yaw轴角度控制，为角度的增量 单位 rad
  * @param[in]      pitch: pitch轴角度控制，为角度的增量 单位 rad
  * @param[in]      gimbal_control_set: 云台数据指针
  * @retval         none
  */
static void gimbal_relative_angle_control(float32_t *yaw, float32_t *pitch, gimbal_control_t *gimbal_control_set);

/**
  * @brief          pid自动调节控制，电机是直接电流控制，
  * @param[in]      yaw: yaw轴给定电流值
  * @param[in]      pitch: pitch轴给定电流值
  * @param[in]      gimbal_control_set: 云台数据指针
  * @retval         none
  */
static void gimbal_pid_auto_tune_control(float32_t *yaw, float32_t *pitch, gimbal_control_t *gimbal_control_set,
                                         const volatile pid_auto_tune_t *pid_auto_tune);

/**
  * @brief          when gimbal behaviour mode is GIMBAL_MOTIONLESS, the function is called
  *                 and gimbal control mode is encode mode. 
  * @param[out]     yaw: yaw axia relative angle increment,  unit rad
  * @param[out]     pitch: pitch axia relative angle increment, unit rad
  * @param[in]      gimbal_control_set: gimbal data
  * @retval         none
  */
/**
  * @brief          云台进入遥控器无输入控制，电机是相对角度控制，
  * @author         RM
  * @param[in]      yaw: yaw轴角度控制，为角度的增量 单位 rad
  * @param[in]      pitch: pitch轴角度控制，为角度的增量 单位 rad
  * @param[in]      gimbal_control_set:云台数据指针
  * @retval         none
  */
static void gimbal_motionless_control(float32_t *yaw, float32_t *pitch, gimbal_control_t *gimbal_control_set);

//云台行为状态机
gimbal_behaviour_e gimbal_behaviour = GIMBAL_ZERO_FORCE;
float32_t yaw_rc_Interpolation[14] = {0};
float32_t pitch_rc_Interpolation[14] = {0};
float32_t yaw_vision_Interpolation[200] = {0};
float32_t pitch_vision_Interpolation[200] = {0};

/**
  * @brief          the function is called by gimbal_set_mode function in gimbal_task.c
  *                 the function set gimbal_behaviour variable, and set motor mode.
  * @param[in]      gimbal_mode_set: gimbal data
  * @retval         none
  */
/**
  * @brief          被gimbal_set_mode函数调用在gimbal_task.c,云台行为状态机以及电机状态机设置
  * @param[out]     gimbal_mode_set: 云台数据指针
  * @retval         none
  */

void gimbal_behaviour_mode_set(gimbal_control_t *gimbal_mode_set) {
    if (gimbal_mode_set == NULL) {
        return;
    }
    //set gimbal_behaviour variable
    //云台行为状态机设置
    gimbal_behavour_set(gimbal_mode_set);

    //accoring to gimbal_behaviour, set motor control mode
    //根据云台行为状态机设置电机状态机
    if (gimbal_behaviour == GIMBAL_ZERO_FORCE) {
        gimbal_mode_set->gimbal_yaw_motor.gimbal_motor_mode = GIMBAL_MOTOR_RAW;
        gimbal_mode_set->gimbal_pitch_motor.gimbal_motor_mode = GIMBAL_MOTOR_RAW;
    } else if (gimbal_behaviour == GIMBAL_PID_AUTO_TUNE) {
        gimbal_mode_set->gimbal_yaw_motor.gimbal_motor_mode = GIMBAL_MOTOR_PID_AUTO_TUNE;
        gimbal_mode_set->gimbal_pitch_motor.gimbal_motor_mode = GIMBAL_MOTOR_PID_AUTO_TUNE;
    } else if (gimbal_behaviour == GIMBAL_INIT) {
        gimbal_mode_set->gimbal_yaw_motor.gimbal_motor_mode = GIMBAL_MOTOR_ENCONDE;
        gimbal_mode_set->gimbal_pitch_motor.gimbal_motor_mode = GIMBAL_MOTOR_ENCONDE;
    } else if (gimbal_behaviour == GIMBAL_CALI) {
        gimbal_mode_set->gimbal_yaw_motor.gimbal_motor_mode = GIMBAL_MOTOR_RAW;
        gimbal_mode_set->gimbal_pitch_motor.gimbal_motor_mode = GIMBAL_MOTOR_RAW;
    } else if (gimbal_behaviour == IST_CALI) {
        gimbal_mode_set->gimbal_yaw_motor.gimbal_motor_mode = GIMBAL_MOTOR_RAW;
        gimbal_mode_set->gimbal_pitch_motor.gimbal_motor_mode = GIMBAL_MOTOR_RAW;
    } else if (gimbal_behaviour == GIMBAL_ABSOLUTE_ANGLE) {
        gimbal_mode_set->gimbal_yaw_motor.gimbal_motor_mode = GIMBAL_MOTOR_GYRO;
        gimbal_mode_set->gimbal_pitch_motor.gimbal_motor_mode = GIMBAL_MOTOR_GYRO;
    } else if (gimbal_behaviour == GIMBAL_RELATIVE_ANGLE) {
        gimbal_mode_set->gimbal_yaw_motor.gimbal_motor_mode = GIMBAL_MOTOR_ENCONDE;
        gimbal_mode_set->gimbal_pitch_motor.gimbal_motor_mode = GIMBAL_MOTOR_ENCONDE;
    } else if (gimbal_behaviour == GIMBAL_MOTIONLESS) {
        gimbal_mode_set->gimbal_yaw_motor.gimbal_motor_mode = GIMBAL_MOTOR_ENCONDE;
        gimbal_mode_set->gimbal_pitch_motor.gimbal_motor_mode = GIMBAL_MOTOR_ENCONDE;
    }
}

/**
  * @brief          the function is called by gimbal_set_contorl function in gimbal_task.c
  *                 accoring to the gimbal_behaviour variable, call the corresponding function
  * @param[out]     add_yaw:yaw axis increment angle, unit rad
  * @param[out]     add_pitch:pitch axis increment angle,unit rad
  * @param[in]      gimbal_mode_set: gimbal data
  * @retval         none
  */
/**
  * @brief          云台行为控制，根据不同行为采用不同控制函数
  * @param[out]     add_yaw:设置的yaw角度增加值，单位 rad
  * @param[out]     add_pitch:设置的pitch角度增加值，单位 rad
  * @param[in]      gimbal_mode_set:云台数据指针
  * @retval         none
  */
void gimbal_behaviour_control_set(float32_t *add_yaw, float32_t *add_pitch, gimbal_control_t *gimbal_control_set) {

    if (add_yaw == NULL || add_pitch == NULL || gimbal_control_set == NULL) {
        return;
    }


    if (gimbal_behaviour == GIMBAL_ZERO_FORCE) {
        gimbal_zero_force_control(add_yaw, add_pitch, gimbal_control_set);
    } else if (gimbal_behaviour == GIMBAL_PID_AUTO_TUNE) {
        gimbal_pid_auto_tune_control(add_yaw, add_pitch, gimbal_control_set,
                                     gimbal_control_set->pid_auto_tune_data_point);
    } else if (gimbal_behaviour == GIMBAL_INIT) {
        gimbal_init_control(add_yaw, add_pitch, gimbal_control_set);
    } else if (gimbal_behaviour == GIMBAL_CALI) {
        gimbal_cali_control(add_yaw, add_pitch, gimbal_control_set);
    } else if (gimbal_behaviour == IST_CALI) {
        ist_cali_control(add_yaw, add_pitch, gimbal_control_set);
    } else if (gimbal_behaviour == GIMBAL_ABSOLUTE_ANGLE) {
        gimbal_absolute_angle_control(add_yaw, add_pitch, gimbal_control_set);
    } else if (gimbal_behaviour == GIMBAL_RELATIVE_ANGLE) {
        gimbal_relative_angle_control(add_yaw, add_pitch, gimbal_control_set);
    } else if (gimbal_behaviour == GIMBAL_MOTIONLESS) {
        gimbal_motionless_control(add_yaw, add_pitch, gimbal_control_set);
    }

}

/**
  * @brief          in some gimbal mode, need chassis keep no move
  * @param[in]      none
  * @retval         1: no move 0:normal
  */
/**
  * @brief          云台在某些行为下，需要底盘不动
  * @param[in]      none
  * @retval         1: no move 0:normal
  */

bool_t gimbal_cmd_to_chassis_stop(void) {
    if (gimbal_behaviour == GIMBAL_INIT || gimbal_behaviour == GIMBAL_CALI || gimbal_behaviour == IST_CALI ||
        gimbal_behaviour == GIMBAL_MOTIONLESS ||
        gimbal_behaviour == GIMBAL_ZERO_FORCE) {
        return 1;
    } else {
        return 0;
    }
}

/**
  * @brief          in some gimbal mode, need shoot keep no move
  * @param[in]      none
  * @retval         1: no move 0:normal
  */
/**
  * @brief          云台在某些行为下，需要射击停止
  * @param[in]      none
  * @retval         1: no move 0:normal
  */

bool_t gimbal_cmd_to_shoot_stop(void) {
    if (gimbal_behaviour == GIMBAL_INIT || gimbal_behaviour == GIMBAL_CALI || gimbal_behaviour == GIMBAL_ZERO_FORCE) {
        return 1;
    } else {
        return 0;
    }
}


/**
  * @brief          gimbal behave mode set.
  * @param[in]      gimbal_mode_set: gimbal data
  * @retval         none
  */
/**
  * @brief          云台行为状态机设置.
  * @param[in]      gimbal_mode_set: 云台数据指针
  * @retval         none
  */
static void gimbal_behavour_set(gimbal_control_t *gimbal_mode_set) {
    if (gimbal_mode_set == NULL) {
        return;
    }
    if (toe_is_error(DBUS_TOE)) {
        gimbal_behaviour = GIMBAL_ZERO_FORCE;
    }
    static gimbal_behaviour_e last_gimbal_behaviour = GIMBAL_ZERO_FORCE;
    static bool_t init_complete_flag = 0;
/***************vision key control start*******************/
//    if ((switch_is_up(gimbal_mode_set->gimbal_rc_ctrl->rc.s[RADIO_CONTROL_SWITCH_L])) &&
//        (switch_is_up(gimbal_mode_set->gimbal_rc_ctrl->rc.s[RADIO_CONTROL_SWITCH_R]))) {
//        if ((gimbal_mode_set->gimbal_rc_ctrl->key.v & KEY_PRESSED_OFFSET_R) &&
//            (global_vision_info.state == VISION_OFF)) {
//            global_vision_info.state = VISION_ON;
//        } else if ((gimbal_mode_set->gimbal_rc_ctrl->key.v & KEY_PRESSED_OFFSET_R) &&
//                   (global_vision_info.state == VISION_ON)) {
//            global_vision_info.state = VISION_OFF;
//        }
//    }
    if (gimbal_mode_set->gimbal_rc_ctrl->mouse.press_r) {
        global_vision_info.state = VISION_ON;
    } else {
        global_vision_info.state = VISION_OFF;
    }

/***************vision key control end*******************/
    if (PID_AUTO_TUNE) {
        gimbal_behaviour = GIMBAL_PID_AUTO_TUNE;
        return;
    }

    if (gimbal_behaviour == GIMBAL_CALI && gimbal_control.gimbal_cali.step == GIMBAL_CALI_END_STEP &&
        cali_sensor[CALI_GIMBAL].cali_done == CALIED_FLAG) {
        gimbal_behaviour = GIMBAL_ZERO_FORCE;
        last_gimbal_behaviour = gimbal_behaviour;
        gimbal_control.gimbal_cali.step = 0;
        return;
    }
    if (gimbal_behaviour == IST_CALI && gimbal_control.ist_cali.step == IST_CALI_END_STEP &&
        cali_sensor[CALI_GYRO_MAG].cali_done == CALIED_FLAG && init_complete_flag) {
        gimbal_behaviour = GIMBAL_ZERO_FORCE;
        last_gimbal_behaviour = gimbal_behaviour;
        init_complete_flag = 0;
        gimbal_control.ist_cali.step = 0;
        return;
    }

    //in cali mode, return
    //校准行为，return 不会设置其他的模式
    if (gimbal_behaviour == GIMBAL_CALI && gimbal_mode_set->gimbal_cali.step != GIMBAL_CALI_END_STEP &&
        !toe_is_error(DBUS_TOE)) {
        return;
    }
    if (gimbal_behaviour == IST_CALI && gimbal_mode_set->ist_cali.step != IST_CALI_END_STEP && init_complete_flag) {
        return;
    }
    //if other operate make step change to start, means enter cali mode
    //如果外部使得校准步骤从0 变成 start，则进入校准模式
    if (gimbal_mode_set->gimbal_cali.step == GIMBAL_CALI_START_STEP && !toe_is_error(DBUS_TOE)) {
        gimbal_behaviour = GIMBAL_CALI;
        return;
    }
    if (gimbal_mode_set->ist_cali.step == IST_CALI_START_STEP && !toe_is_error(DBUS_TOE)) {
        if (init_complete_flag) {
            gimbal_behaviour = IST_CALI;
            return;
        } else if (!init_complete_flag) {
            gimbal_behaviour = GIMBAL_INIT;
        }
    }
    //init mode, judge if gimbal is in middle place
    //初始化模式判断是否到达中值位置
    if (gimbal_behaviour == GIMBAL_INIT) {
        static uint16_t init_time = 0;
        static uint16_t init_stop_time = 0;
        init_time++;

        if (gimbal_mode_set->gimbal_yaw_motor.relative_angle - INIT_YAW_SET <
            GIMBAL_INIT_ANGLE_ERROR &&
            gimbal_mode_set->gimbal_yaw_motor.relative_angle - INIT_YAW_SET <
            GIMBAL_INIT_ANGLE_ERROR) {

            if (init_stop_time < GIMBAL_INIT_STOP_TIME) {
                init_stop_time++;
            }
        } else {

            if (init_time < GIMBAL_INIT_TIME) {
                init_time++;
            }
        }

        //超过初始化最大时间，或者已经稳定到中值一段时间，退出初始化状态开关打下档，或者掉线
        if (INIT_ONLY_FIRST_TIME) {
            if (init_time < GIMBAL_INIT_TIME && init_stop_time < GIMBAL_INIT_STOP_TIME &&
                !toe_is_error(DBUS_TOE) && !init_complete_flag) {
                return;
            } else {
                if (init_stop_time == GIMBAL_INIT_STOP_TIME) {
                    init_complete_flag = 1;
                }
                init_stop_time = 0;
                init_time = 0;
            }
        } else {
            if (init_time < GIMBAL_INIT_TIME && init_stop_time < GIMBAL_INIT_STOP_TIME &&
                !toe_is_error(DBUS_TOE)) {
                return;
            } else {
                if (init_stop_time == GIMBAL_INIT_STOP_TIME) {
                    init_complete_flag = 1;
                }
                init_stop_time = 0;
                init_time = 0;
            }
        }

    }


    //开关控制 云台状态
    if ((switch_is_mid(gimbal_mode_set->gimbal_rc_ctrl->rc.s[RADIO_CONTROL_SWITCH_L]) &&
         switch_is_down(gimbal_mode_set->gimbal_rc_ctrl->rc.s[RADIO_CONTROL_SWITCH_R])) ||
        ((switch_is_up(gimbal_mode_set->gimbal_rc_ctrl->rc.s[RADIO_CONTROL_SWITCH_L])) &&
         (switch_is_up(gimbal_mode_set->gimbal_rc_ctrl->rc.s[RADIO_CONTROL_SWITCH_R])) &&
         (gimbal_mode_set->gimbal_rc_ctrl->key.v & KEY_PRESSED_OFFSET_G))) {
//        gimbal_behaviour = GIMBAL_ZERO_FORCE;
        gimbal_behaviour = GIMBAL_RELATIVE_ANGLE;
    } else if ((switch_is_mid(gimbal_mode_set->gimbal_rc_ctrl->rc.s[RADIO_CONTROL_SWITCH_L]) &&
                switch_is_mid(gimbal_mode_set->gimbal_rc_ctrl->rc.s[RADIO_CONTROL_SWITCH_R])) ||
               ((switch_is_up(gimbal_mode_set->gimbal_rc_ctrl->rc.s[RADIO_CONTROL_SWITCH_L])) &&
                (switch_is_up(gimbal_mode_set->gimbal_rc_ctrl->rc.s[RADIO_CONTROL_SWITCH_R])) &&
                (gimbal_mode_set->gimbal_rc_ctrl->key.v & KEY_PRESSED_OFFSET_F))) {
        gimbal_behaviour = GIMBAL_ABSOLUTE_ANGLE;
    } else if ((switch_is_mid(gimbal_mode_set->gimbal_rc_ctrl->rc.s[RADIO_CONTROL_SWITCH_L]) &&
                switch_is_up(gimbal_mode_set->gimbal_rc_ctrl->rc.s[RADIO_CONTROL_SWITCH_R])) ||
               ((switch_is_up(gimbal_mode_set->gimbal_rc_ctrl->rc.s[RADIO_CONTROL_SWITCH_L])) &&
                (switch_is_up(gimbal_mode_set->gimbal_rc_ctrl->rc.s[RADIO_CONTROL_SWITCH_R])) &&
                (gimbal_mode_set->gimbal_rc_ctrl->key.v & KEY_PRESSED_OFFSET_R))) {
//        gimbal_behaviour = GIMBAL_ABSOLUTE_ANGLE;
        gimbal_behaviour = GIMBAL_ABSOLUTE_ANGLE;
    } else if ((switch_is_up(gimbal_mode_set->gimbal_rc_ctrl->rc.s[RADIO_CONTROL_SWITCH_L])) &&
               (switch_is_up(gimbal_mode_set->gimbal_rc_ctrl->rc.s[RADIO_CONTROL_SWITCH_R]))) {
        gimbal_behaviour = GIMBAL_ABSOLUTE_ANGLE;
    }

//    SEGGER_RTT_printf(0,"b=%d\r\n",gimbal_behaviour);

    if (toe_is_error(DBUS_TOE)) {
        gimbal_behaviour = GIMBAL_ZERO_FORCE;
    }

    //enter init mode
    //判断进入init状态机

    if (last_gimbal_behaviour == GIMBAL_ZERO_FORCE && gimbal_behaviour != GIMBAL_ZERO_FORCE) {
        if (INIT_ONLY_FIRST_TIME) {
            if (!init_complete_flag) {
                gimbal_behaviour = GIMBAL_INIT;
            }
        } else {
            gimbal_behaviour = GIMBAL_INIT;
        }

    }
    last_gimbal_behaviour = gimbal_behaviour;
}

/**
  * @brief          when gimbal behaviour mode is GIMBAL_ZERO_FORCE, the function is called
  *                 and gimbal control mode is raw. The raw mode means set value
  *                 will be sent to CAN bus derectly, and the function will set all zero.
  * @param[out]     yaw: yaw motor current set, it will be sent to CAN bus derectly.
  * @param[out]     pitch: pitch motor current set, it will be sent to CAN bus derectly.
  * @param[in]      gimbal_control_set: gimbal data
  * @retval         none
  */
/**
  * @brief          当云台行为模式是GIMBAL_ZERO_FORCE, 这个函数会被调用,云台控制模式是raw模式.原始模式意味着
  *                 设定值会直接发送到CAN总线上,这个函数将会设置所有为0.
  * @param[in]      yaw:发送yaw电机的原始值，会直接通过can 发送到电机
  * @param[in]      pitch:发送pitch电机的原始值，会直接通过can 发送到电机
  * @param[in]      gimbal_control_set: 云台数据指针
  * @retval         none
  */
static void gimbal_zero_force_control(float32_t *yaw, float32_t *pitch, gimbal_control_t *gimbal_control_set) {
    if (yaw == NULL || pitch == NULL || gimbal_control_set == NULL) {
        return;
    }

    *yaw = 0.0f;
    *pitch = 0.0f;
}
/**
  * @brief          when gimbal behaviour mode is GIMBAL_INIT, the function is called
  *                 and gimbal control mode is gyro mode. gimbal will lift the pitch axis
  *                 and rotate yaw axis.
  * @param[out]     yaw: yaw motor relative angle increment, unit rad.
  * @param[out]     pitch: pitch motor absolute angle increment, unit rad.
  * @param[in]      gimbal_control_set: gimbal data
  * @retval         none
  */
/**
  * @brief          云台初始化控制，电机是陀螺仪角度控制，云台先抬起pitch轴，后旋转yaw轴
  * @author         RM
  * @param[out]     yaw轴角度控制，为角度的增量 单位 rad
  * @param[out]     pitch轴角度控制，为角度的增量 单位 rad
  * @param[in]      云台数据指针
  * @retval         返回空
  */
static void gimbal_init_control(float32_t *yaw, float32_t *pitch, gimbal_control_t *gimbal_control_set) {
    if (yaw == NULL || pitch == NULL || gimbal_control_set == NULL) {
        return;
    }

    //初始化状态控制量计算
    if (fabsf(INIT_PITCH_SET - gimbal_control_set->gimbal_pitch_motor.relative_angle) > GIMBAL_INIT_PITCH_ANGLE_LIMIT) {
        if (PITCH_INIT == INIT) {
            *pitch = (INIT_PITCH_SET - gimbal_control_set->gimbal_pitch_motor.relative_angle) * GIMBAL_INIT_PITCH_SPEED;
            *yaw = 0.0f;
//            SEGGER_RTT_printf(0, "angle=%f\r\n", gimbal_control_set->gimbal_pitch_motor.relative_angle);
//            *yaw = (INIT_YAW_SET - gimbal_control_set->gimbal_yaw_motor.relative_angle) * GIMBAL_INIT_YAW_SPEED;
        } else if (PITCH_INIT == NO_INIT) {
            *pitch = 0.0f;
            *yaw = 0.0f;
        }
    } else {
        if (PITCH_INIT == INIT) {
            *pitch = (INIT_PITCH_SET - gimbal_control_set->gimbal_pitch_motor.relative_angle) * GIMBAL_INIT_PITCH_SPEED;
//            SEGGER_RTT_printf(0, "angle=%f\r\n", gimbal_control_set->gimbal_pitch_motor.relative_angle);
        } else if (PITCH_INIT == NO_INIT) {
            *pitch = 0.0f;
        }
        if (YAW_INIT == INIT) {
            *yaw = jump_error(INIT_YAW_SET - gimbal_control_set->gimbal_yaw_motor.relative_angle, 2 * PI) *
                   GIMBAL_INIT_YAW_SPEED;
//            SEGGER_RTT_printf(0,"%f\r\n",jump_error(INIT_YAW_SET - gimbal_control_set->gimbal_yaw_motor.relative_angle,2*PI));
        } else if (YAW_INIT == NO_INIT) {
            *yaw = 0.0f;
        }
    }
}

/**
  * @brief          when gimbal behaviour mode is GIMBAL_CALI, the function is called
  *                 and gimbal control mode is raw mode. gimbal will lift the pitch axis, 
  *                 and then put down the pitch axis, and rotate yaw axis counterclockwise,
  *                 and rotate yaw axis clockwise.
  * @param[out]     yaw: yaw motor current set, will be sent to CAN bus decretly
  * @param[out]     pitch: pitch motor current set, will be sent to CAN bus decretly
  * @param[in]      gimbal_control_set: gimbal data
  * @retval         none
  */
/**
  * @brief          云台校准控制，电机是raw控制，云台先抬起pitch，放下pitch，在正转yaw，最后反转yaw，记录当时的角度和编码值
  * @author         RM
  * @param[out]     yaw:发送yaw电机的原始值，会直接通过can 发送到电机
  * @param[out]     pitch:发送pitch电机的原始值，会直接通过can 发送到电机
  * @param[in]      gimbal_control_set:云台数据指针
  * @retval         none
  */
static void gimbal_cali_control(float32_t *yaw, float32_t *pitch, gimbal_control_t *gimbal_control_set) {
    if (yaw == NULL || pitch == NULL || gimbal_control_set == NULL) {
        return;
    }
    static uint16_t cali_time = 0;

    if (gimbal_control_set->gimbal_cali.step == GIMBAL_CALI_PITCH_MAX_STEP) {

        *pitch = GIMBAL_CALI_MOTOR_SET;
        *yaw = 0;

        //判断陀螺仪数据， 并记录最大最小角度数据
        gimbal_cali_gyro_judge(gimbal_control_set->gimbal_pitch_motor.motor_gyro, cali_time,
                               gimbal_control_set->gimbal_cali.max_pitch,
                               AHRS_get_instant_pitch(),
                               gimbal_control_set->gimbal_cali.max_pitch_ecd,
                               gimbal_control_set->gimbal_pitch_motor.gimbal_motor_measure->total_ecd,
                               gimbal_control_set->gimbal_cali.step);
    } else if (gimbal_control_set->gimbal_cali.step == GIMBAL_CALI_PITCH_MIN_STEP) {
        *pitch = -GIMBAL_CALI_MOTOR_SET;
        *yaw = 0;

        gimbal_cali_gyro_judge(gimbal_control_set->gimbal_pitch_motor.motor_gyro, cali_time,
                               gimbal_control_set->gimbal_cali.min_pitch,
                               AHRS_get_instant_pitch(),
                               gimbal_control_set->gimbal_cali.min_pitch_ecd,
                               gimbal_control_set->gimbal_pitch_motor.gimbal_motor_measure->total_ecd,
                               gimbal_control_set->gimbal_cali.step);
    } else if (gimbal_control_set->gimbal_cali.step == GIMBAL_CALI_YAW_MAX_STEP) {
        *pitch = 0;
        if (YAW_LIMIT == YAW_HAVE_LIMIT) {
            *yaw = GIMBAL_CALI_MOTOR_SET;
            //yaw absolute_angle is not usefull
            gimbal_cali_gyro_judge(gimbal_control_set->gimbal_yaw_motor.motor_gyro, cali_time,
                                   gimbal_control_set->gimbal_cali.max_yaw,
                                   gimbal_control_set->gimbal_yaw_motor.absolute_angle,
                                   gimbal_control_set->gimbal_cali.max_yaw_ecd,
                                   gimbal_control_set->gimbal_yaw_motor.gimbal_motor_measure->total_ecd,
                                   gimbal_control_set->gimbal_cali.step);
        } else if (YAW_LIMIT == YAW_NO_LIMIT) {
            *yaw = 0;
            gimbal_control_set->gimbal_cali.max_yaw = 0.0f;
            gimbal_control_set->gimbal_cali.max_yaw_ecd = gimbal_control_set->gimbal_yaw_motor.gimbal_motor_measure->total_ecd;
            gimbal_control_set->gimbal_cali.step++;
        }

    } else if (gimbal_control_set->gimbal_cali.step == GIMBAL_CALI_YAW_MIN_STEP) {
        *pitch = 0;
        if (YAW_LIMIT == YAW_HAVE_LIMIT) {
            *yaw = -GIMBAL_CALI_MOTOR_SET;
            //yaw absolute_angle is not usefull
            gimbal_cali_gyro_judge(gimbal_control_set->gimbal_yaw_motor.motor_gyro, cali_time,
                                   gimbal_control_set->gimbal_cali.min_yaw,
                                   gimbal_control_set->gimbal_yaw_motor.absolute_angle,
                                   gimbal_control_set->gimbal_cali.min_yaw_ecd,
                                   gimbal_control_set->gimbal_yaw_motor.gimbal_motor_measure->total_ecd,
                                   gimbal_control_set->gimbal_cali.step);
        } else if (YAW_LIMIT == YAW_NO_LIMIT) {
            *yaw = 0;
            gimbal_control_set->gimbal_cali.min_yaw = 0.0f;
            gimbal_control_set->gimbal_cali.min_yaw_ecd = gimbal_control_set->gimbal_yaw_motor.gimbal_motor_measure->total_ecd;
            gimbal_control_set->gimbal_cali.step++;
        }
    } else if (gimbal_control_set->gimbal_cali.step == GIMBAL_CALI_END_STEP) {
        cali_time = 0;
    }
}

static void ist_cali_control(float32_t *yaw, float32_t *pitch, gimbal_control_t *gimbal_control_set) {
    if (yaw == NULL || pitch == NULL || gimbal_control_set == NULL) {
        return;
    }
    if (gimbal_control_set->ist_cali.step == IST_CALI_FORWARD_STEP) {

        *pitch = 0;
        *yaw = IST_CALI_YAW_MOTOR_SET;
#if YAW_TURN
        if (gimbal_control_set->gimbal_yaw_motor.gimbal_motor_measure->turnCount < -1) {
            gimbal_control_set->ist_cali.step++;
        }
#else
        if(gimbal_control_set->gimbal_yaw_motor.gimbal_motor_measure->turnCount > 1){
            gimbal_control_set->ist_cali.step++;
        }
#endif
    } else if (gimbal_control_set->ist_cali.step == IST_CALI_BACKWARD_STEP) {
        *pitch = 0;
        *yaw = -IST_CALI_YAW_MOTOR_SET;

#if YAW_TURN
        if (gimbal_control_set->gimbal_yaw_motor.gimbal_motor_measure->turnCount > 1) {
            gimbal_control_set->ist_cali.step++;
        }
#else
        if(gimbal_control_set->gimbal_yaw_motor.gimbal_motor_measure->turnCount < -1){
            gimbal_control_set->ist_cali.step++;
        }
#endif
    } else if (gimbal_control_set->ist_cali.step == IST_CALI_END_STEP) {
        return;
    }
}

/**
  * @brief          when gimbal behaviour mode is GIMBAL_ABSOLUTE_ANGLE, the function is called
  *                 and gimbal control mode is gyro mode. 
  * @param[out]     yaw: yaw axia absolute angle increment, unit rad
  * @param[out]     pitch: pitch axia absolute angle increment,unit rad
  * @param[in]      gimbal_control_set: gimbal data
  * @retval         none
  */
/**
  * @brief          云台陀螺仪控制，电机是陀螺仪角度控制，
  * @param[out]     yaw: yaw轴角度控制，为角度的增量 单位 rad
  * @param[out]     pitch:pitch轴角度控制，为角度的增量 单位 rad
  * @param[in]      gimbal_control_set:云台数据指针
  * @retval         none
  */
static void gimbal_absolute_angle_control(float32_t *yaw, float32_t *pitch, gimbal_control_t *gimbal_control_set) {
    if (yaw == NULL || pitch == NULL || gimbal_control_set == NULL) {
        return;
    }

    gimbal_rc_to_control_vector(yaw, pitch, gimbal_control_set);


//    {
    static uint16_t last_turn_keyboard = 0;
    static uint8_t gimbal_turn_flag = 0;
    static float32_t gimbal_end_angle = 0.0f;

//        if ((gimbal_control_set->gimbal_rc_ctrl->key.v & TURN_KEYBOARD) && !(last_turn_keyboard & TURN_KEYBOARD))
//        {
//            if (gimbal_turn_flag == 0)
//            {
//                gimbal_turn_flag = 1;
//                //保存掉头的目标值
//                gimbal_end_angle = rad_format(gimbal_control_set->gimbal_yaw_motor.absolute_angle + PI);
//            }
//        }
//        last_turn_keyboard = gimbal_control_set->gimbal_rc_ctrl->key.v ;
//
//        if (gimbal_turn_flag)
//        {
//            //不断控制到掉头的目标值，正转，反装是随机
//            if (rad_format(gimbal_end_angle - gimbal_control_set->gimbal_yaw_motor.absolute_angle) > 0.0f)
//            {
//                *yaw += TURN_SPEED;
//            }
//            else
//            {
//                *yaw -= TURN_SPEED;
//            }
//        }
//        //到达pi （180°）后停止
//        if (gimbal_turn_flag && fabs(rad_format(gimbal_end_angle - gimbal_control_set->gimbal_yaw_motor.absolute_angle)) < 0.01f)
//        {
//            gimbal_turn_flag = 0;
//        }
//    }
}

/**
  * @brief          pid自动调节控制，电机是直接电流控制，
  * @param[in]      yaw: yaw轴给定电流值
  * @param[in]      pitch: pitch轴给定电流值
  * @param[in]      gimbal_control_set: 云台数据指针
  * @retval         none
  */
static void gimbal_pid_auto_tune_control(float32_t *yaw, float32_t *pitch, gimbal_control_t *gimbal_control_set,
                                         const volatile pid_auto_tune_t *pid_auto_tune) {
    if (yaw == NULL || pitch == NULL || gimbal_control_set == NULL) {
        return;
    }
    *yaw = pid_auto_tune->control_list.yaw_control_value;
    *pitch = pid_auto_tune->control_list.pitch_control_value;
}


/**
  * @brief          when gimbal behaviour mode is GIMBAL_RELATIVE_ANGLE, the function is called
  *                 and gimbal control mode is encode mode. 
  * @param[out]     yaw: yaw axia relative angle increment, unit rad
  * @param[out]     pitch: pitch axia relative angle increment,unit rad
  * @param[in]      gimbal_control_set: gimbal data
  * @retval         none
  */
/**
  * @brief          云台编码值控制，电机是相对角度控制，
  * @param[in]      yaw: yaw轴角度控制，为角度的增量 单位 rad
  * @param[in]      pitch: pitch轴角度控制，为角度的增量 单位 rad
  * @param[in]      gimbal_control_set: 云台数据指针
  * @retval         none
  */
static void gimbal_relative_angle_control(float32_t *yaw, float32_t *pitch, gimbal_control_t *gimbal_control_set) {
    if (yaw == NULL || pitch == NULL || gimbal_control_set == NULL) {
        return;
    }
    gimbal_rc_to_control_vector(yaw, pitch, gimbal_control_set);
}

/**
  * @brief          when gimbal behaviour mode is GIMBAL_MOTIONLESS, the function is called
  *                 and gimbal control mode is encode mode. 
  * @param[out]     yaw: yaw axia relative angle increment,  unit rad
  * @param[out]     pitch: pitch axia relative angle increment, unit rad
  * @param[in]      gimbal_control_set: gimbal data
  * @retval         none
  */
/**
  * @brief          云台进入遥控器无输入控制，电机是相对角度控制，
  * @author         RM
  * @param[in]      yaw: yaw轴角度控制，为角度的增量 单位 rad
  * @param[in]      pitch: pitch轴角度控制，为角度的增量 单位 rad
  * @param[in]      gimbal_control_set:云台数据指针
  * @retval         none
  */
static void gimbal_motionless_control(float32_t *yaw, float32_t *pitch, gimbal_control_t *gimbal_control_set) {
    if (yaw == NULL || pitch == NULL || gimbal_control_set == NULL) {
        return;
    }
    *yaw = 0.0f;
    *pitch = 0.0f;
}


/**
  * @brief          根据遥控器通道值，计算yaw和pitch控制量
  *
  * @param[out]     yaw: yaw轴角度控制，为角度的增量 单位 rad
  * @param[out]     pitch: pitch轴角度控制，为角度的增量 单位 rad
  * @param[out]     gimbal_control_set: 云台数据指针
  * @retval         none
  */
void gimbal_rc_to_control_vector(float32_t *yaw, float32_t *pitch, gimbal_control_t *gimbal_move_rc_to_vector) {
    if (gimbal_move_rc_to_vector == NULL || yaw == NULL || pitch == NULL) {
        return;
    }
    static bool_t micro_pitch_lock = 0;
    static uint8_t rc_interpolation_num = 10;
    static uint8_t vision_yaw_interpolation_num = 1;
    static uint8_t vision_pitch_interpolation_num = 1;
    static bool_t no_bias_flag = 0;
    static uint8_t rc_move_point = 0;
    static uint8_t vision_move_point = 0;
    static int16_t yaw_rc_last;
    float32_t yaw_bias = 0;
    static float32_t last_yaw_bias = 0;
    float32_t pitch_bias = 0;
    int16_t err;
    int16_t yaw_channel, pitch_channel;
    float32_t yaw_set_channel, pitch_set_channel, add_vision_yaw, add_vision_pitch, lim_vision_yaw, lim_vision_pitch, micro_pitch_add;
    yaw_set_channel = pitch_set_channel = add_vision_yaw = add_vision_pitch = yaw_channel = pitch_channel = micro_pitch_add = 0;
    //视觉控制
    if (gimbal_move_rc_to_vector->gimbal_vision_ctrl->update_flag) {
        clear_vision_update_flag();
        memset(yaw_vision_Interpolation, 0, sizeof(yaw_vision_Interpolation));
        memset(yaw_vision_Interpolation, 0, sizeof(yaw_vision_Interpolation));
        rc_move_point = 0;
//        SEGGER_RTT_printf(0, "%d,%f,%f\r\n", gimbal_move_rc_to_vector->gimbal_vision_ctrl->fps,
//                          gimbal_move_rc_to_vector->gimbal_vision_ctrl->yaw_angle,
//                          gimbal_move_rc_to_vector->gimbal_vision_ctrl->pitch_angle);
        if (gimbal_move_rc_to_vector->gimbal_vision_ctrl->fps <= 10) {
            global_vision_info.vision_control.fps = 20;
        } else if (gimbal_move_rc_to_vector->gimbal_vision_ctrl->fps > 60) {
            global_vision_info.vision_control.fps = 60;
        }
//        SEGGER_RTT_WriteString(0,"IN\r\n");
        if (fabsf(add_vision_yaw) > 2.0f) {
            vision_yaw_interpolation_num = 3;
        } else {
            vision_yaw_interpolation_num = 2;
        }
        if (fabsf(add_vision_pitch) > 2.0f) {
            vision_pitch_interpolation_num = 3;
        } else {
            vision_pitch_interpolation_num = 2;
        }
        sigmoidInterpolation(0,
                             (gimbal_move_rc_to_vector->gimbal_vision_ctrl->yaw_angle / vision_yaw_interpolation_num) *
                             1000,
                             (1000 / gimbal_move_rc_to_vector->gimbal_vision_ctrl->fps) * 2,
                             yaw_vision_Interpolation);
        sigmoidInterpolation(0, (gimbal_move_rc_to_vector->gimbal_vision_ctrl->pitch_angle /
                                 vision_pitch_interpolation_num) * 1000,
                             (1000 / gimbal_move_rc_to_vector->gimbal_vision_ctrl->fps) * 2,
                             pitch_vision_Interpolation);
    }
    add_vision_yaw = yaw_vision_Interpolation[vision_move_point] / 1000;
    add_vision_pitch = pitch_vision_Interpolation[vision_move_point] / 1000;
    vision_move_point++;
    if (vision_move_point > (1000 / gimbal_move_rc_to_vector->gimbal_vision_ctrl->fps) - 1) {
        vision_move_point = 0;
    }
//    SEGGER_RTT_printf(0, "%d,%f,%f\r\n", gimbal_move_rc_to_vector->gimbal_vision_ctrl->fps,
//                      add_vision_yaw,
//                      add_vision_pitch);
//limit
    if (gimbal_behaviour == GIMBAL_RELATIVE_ANGLE) {
        rc_interpolation_num = 50;
    } else if (gimbal_behaviour == GIMBAL_ABSOLUTE_ANGLE) {
        if (chassis_behaviour_mode == CHASSIS_FORWARD_FOLLOW_GIMBAL_YAW) {
            rc_interpolation_num = 80;
        } else if (chassis_behaviour_mode == CHASSIS_SPIN) {
            rc_interpolation_num = 25;
        }
    } else {
        rc_interpolation_num = 20;
    }
    if (gimbal_behaviour == GIMBAL_RELATIVE_ANGLE) {
        yaw_bias = jump_error(
                gimbal_move_rc_to_vector->gimbal_yaw_motor.relative_angle_set -
                gimbal_move_rc_to_vector->gimbal_yaw_motor.relative_angle, 2 * PI);
//            yaw_bias = 0.0f
        pitch_bias = jump_error(
                gimbal_move_rc_to_vector->gimbal_pitch_motor.relative_angle_set -
                gimbal_move_rc_to_vector->gimbal_pitch_motor.relative_angle, 2 * PI);
    } else if (gimbal_behaviour == GIMBAL_ABSOLUTE_ANGLE) {
        yaw_bias = jump_error(
                gimbal_move_rc_to_vector->gimbal_yaw_motor.absolute_angle_set -
                gimbal_move_rc_to_vector->gimbal_yaw_motor.absolute_angle, 2 * PI);
        pitch_bias = jump_error(
                gimbal_move_rc_to_vector->gimbal_pitch_motor.absolute_angle_set -
                gimbal_move_rc_to_vector->gimbal_pitch_motor.absolute_angle, 2 * PI);
    } else {
        yaw_bias = 0;
        pitch_bias = 0;
    }
    if ((fabs(yaw_bias) < 0.01f) || (fabs(yaw_bias)) > 1.5f || yaw_channel == 0) {
        yaw_bias = 0;
        no_bias_flag = 1;
    }
    if ((fabs(pitch_bias) < 0.1f) || (fabs(pitch_bias)) > 1.5f) {
        pitch_bias = 0;
        no_bias_flag = 1;
    }
//        if (chassis_mode_change_flag) {
//            static uint16_t count = 0;
//            if (count < GIMBAL_CONTROL_TIME * 2000) {
//                rc_interpolation_num = 1;
//                no_bias_flag = 1;
//            } else {
//                chassis_mode_change_flag = 0;
//            }
//            count++;
//        }


    if (!switch_is_up(gimbal_move_rc_to_vector->gimbal_rc_ctrl->rc.s[RADIO_CONTROL_SWITCH_L])) {

        //deadline, because some remote control need be calibrated,  the value of rocker is not zero in middle place,
        //死区限制，因为遥控器可能存在差异 摇杆在中间，其值不为0
        rc_deadband_limit(gimbal_move_rc_to_vector->gimbal_rc_ctrl->rc.ch[YAW_CHANNEL], yaw_channel, RC_DEADBAND);
        rc_deadband_limit(gimbal_move_rc_to_vector->gimbal_rc_ctrl->rc.ch[PITCH_CHANNEL], pitch_channel, RC_DEADBAND);
        if (gimbal_move_rc_to_vector->gimbal_rc_ctrl->gimbal_update_flag ||
            (gimbal_move_rc_to_vector->gimbal_rc_ctrl->gimbal_update_flag && no_bias_flag)) {
            clear_gimbal_rc_update_flag();
//            sigmoidInterpolation(0, yaw_channel, 14, yaw_rc_Interpolation);
//            sigmoidInterpolation(0, pitch_channel, 14, pitch_rc_Interpolation);
            if (chassis_behaviour_mode == CHASSIS_FORWARD_FOLLOW_GIMBAL_YAW) {
                sigmoidInterpolation(0, (yaw_channel * YAW_RC_SEN) * 1000, 14,
                                     yaw_rc_Interpolation);
            } else if (chassis_behaviour_mode == CHASSIS_SPIN) {
                static uint16_t count = 0;
                if (spin_pid_change_flag) {
                    sigmoidInterpolation(0, (yaw_channel * YAW_RC_SEN + yaw_bias) * 1000, 14,
                                         yaw_rc_Interpolation);
                    count++;
                    if (count > 10) {
                        spin_pid_change_flag = 0;
                        count = 0;
                        yaw_bias = 0;
                    }
                    last_yaw_bias = yaw_bias;
                } else {
                    if (last_yaw_bias != 0) {
                        float32_t yaw_bias_err = yaw_bias - last_yaw_bias;
                        if (yaw_bias_err > 0) {
                            sigmoidInterpolation(0, (yaw_channel * YAW_RC_SEN) *
                                                    1000,
                                                 14,
                                                 yaw_rc_Interpolation);
                        } else {
                            sigmoidInterpolation(0, (yaw_channel * YAW_RC_SEN) * 1000, 14,
                                                 yaw_rc_Interpolation);
                        }
//                    SEGGER_RTT_printf(0, "chanel=%f,bias=%f\r\n", yaw_channel * YAW_RC_SEN, yaw_bias_err);
                    } else {
                        sigmoidInterpolation(0, (yaw_channel * YAW_RC_SEN) * 1000, 14,
                                             yaw_rc_Interpolation);
                    }
                    last_yaw_bias = yaw_bias;
                }
            } else {
                sigmoidInterpolation(0, (yaw_channel * YAW_RC_SEN) * 1000, 14,
                                     yaw_rc_Interpolation);
            }
            sigmoidInterpolation(0, (pitch_channel * PITCH_RC_SEN) * 1000, 14,
                                 pitch_rc_Interpolation);
            rc_move_point = 0;
            no_bias_flag = 0;
        }
        yaw_set_channel = yaw_rc_Interpolation[rc_move_point] / 1000;
        pitch_set_channel = pitch_rc_Interpolation[rc_move_point] / 1000;
        rc_move_point++;
        if (rc_move_point > 13) {
            rc_move_point = 0;
        }
//        yaw_set_channel = yaw_channel * YAW_RC_SEN/14.0f +
//                          (vision_yaw * (((660 - abs(yaw_channel)) * YAW_RC_SEN) / (660 * YAW_RC_SEN)));
//        pitch_set_channel = pitch_channel * PITCH_RC_SEN + add_vision_pitch;
//        yaw_set_channel = yaw_channel * YAW_RC_SEN;
//        pitch_set_channel = pitch_channel * PITCH_RC_SEN;
    } else {
        static uint16_t key_count = 0;
        //keyboard set speed set-point
        //键盘控制
        rc_deadband_limit(gimbal_move_rc_to_vector->gimbal_rc_ctrl->mouse.x, yaw_channel, 3)
        rc_deadband_limit(gimbal_move_rc_to_vector->gimbal_rc_ctrl->mouse.y, pitch_channel, 2)
        if (gimbal_move_rc_to_vector->gimbal_rc_ctrl->gimbal_update_flag ||
            (gimbal_move_rc_to_vector->gimbal_rc_ctrl->gimbal_update_flag && no_bias_flag)) {
            clear_gimbal_rc_update_flag();
//            sigmoidInterpolation(0, yaw_channel, 14, yaw_rc_Interpolation);
//            sigmoidInterpolation(0, pitch_channel, 14, pitch_rc_Interpolation);
            if (chassis_behaviour_mode == CHASSIS_FORWARD_FOLLOW_GIMBAL_YAW) {
                sigmoidInterpolation(0, (yaw_channel * YAW_MOUSE_SEN) * 1000, 14,
                                     yaw_rc_Interpolation);
            } else if (chassis_behaviour_mode == CHASSIS_SPIN) {
                if (last_yaw_bias != 0) {
                    float32_t yaw_bias_err = yaw_bias - last_yaw_bias;
                    if (yaw_bias_err > 0) {
                        sigmoidInterpolation(0,
                                             (yaw_channel * YAW_MOUSE_SEN + yaw_bias_err / rc_interpolation_num) * 1000,
                                             14,
                                             yaw_rc_Interpolation);
                    } else {
                        sigmoidInterpolation(0, (yaw_channel * YAW_MOUSE_SEN) * 1000, 14,
                                             yaw_rc_Interpolation);
                    }
//                    SEGGER_RTT_printf(0, "chanel=%f,bias=%f\r\n", yaw_channel * YAW_RC_SEN, yaw_bias_err);
                } else {
                    sigmoidInterpolation(0, (yaw_channel * YAW_MOUSE_SEN) * 1000, 14,
                                         yaw_rc_Interpolation);
                }
                last_yaw_bias = yaw_bias;
            } else {
                sigmoidInterpolation(0, (yaw_channel * YAW_MOUSE_SEN) * 1000, 14,
                                     yaw_rc_Interpolation);
            }
            sigmoidInterpolation(0, (pitch_channel * PITCH_MOUSE_SEN) * 1000, 14,
                                 pitch_rc_Interpolation);
            rc_move_point = 0;
            no_bias_flag = 0;
        }
        yaw_set_channel = yaw_rc_Interpolation[rc_move_point] / 1000;
        pitch_set_channel = pitch_rc_Interpolation[rc_move_point] / 1000;
        rc_move_point++;
        if (rc_move_point > 13) {
            rc_move_point = 0;
        }
        if ((gimbal_move_rc_to_vector->gimbal_rc_ctrl->key.v & (KEY_PRESSED_OFFSET_Z)) && key_count == 0) {
            key_count = 150;
            micro_pitch_add = -MICRO_PITCH_ADD_ANGLE;
        }
        if ((gimbal_move_rc_to_vector->gimbal_rc_ctrl->key.v & (KEY_PRESSED_OFFSET_X)) && key_count == 0) {
            key_count = 150;
            micro_pitch_add = MICRO_PITCH_ADD_ANGLE;
        }
        if ((gimbal_move_rc_to_vector->gimbal_rc_ctrl->key.v & (KEY_PRESSED_OFFSET_V)) && (key_count == 0) &&
            (micro_pitch_lock == 0)) {
            key_count = 150;
            micro_pitch_lock = 1;
        } else if ((gimbal_move_rc_to_vector->gimbal_rc_ctrl->key.v & (KEY_PRESSED_OFFSET_V)) && (key_count == 0) &&
                   (micro_pitch_lock == 1)) {
            key_count = 150;
            micro_pitch_lock = 0;
        }
        if (key_count > 0) {
            key_count--;
        }
    }

//for_test
//    RTT_PrintWave(2,
//                  &gimbal_control.gimbal_yaw_motor.relative_angle_set,
//                  &gimbal_control.gimbal_yaw_motor.relative_angle);
    yaw_rc_last = yaw_channel;
    if (global_vision_info.state == VISION_ON) {
        *yaw = add_vision_yaw;
        *pitch = add_vision_pitch;
    } else {
        *yaw = yaw_set_channel;
        if (micro_pitch_lock) {
            *pitch = micro_pitch_add;
        } else {
            *pitch = pitch_set_channel + micro_pitch_add;
        }
    }
}