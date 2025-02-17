/**
  ****************************(C) COPYRIGHT 2019 DJI****************************
  * @file       chassis.c/h
  * @brief      chassis control task,
  *             底盘控制任务
  * @note       
  * @history
  *  Version    Date            Author          Modification
  *  V1.0.0     Dec-26-2018     RM              1. 完成
  *  V1.1.0     Nov-11-2019     RM              1. add chassis power control
  *
  @verbatim
  ==============================================================================

  ==============================================================================
  @endverbatim
  ****************************(C) COPYRIGHT 2019 DJI****************************
  */
#ifndef CHASSIS_TASK_H
#define CHASSIS_TASK_H

#include "struct_typedef.h"
#include "gimbal_task.h"

//in the beginning of task ,wait a time
//任务开始空闲一段时间
#define CHASSIS_TASK_INIT_TIME 357

//the channel num of controlling vertial speed 
//前后的遥控器通道号码
#define CHASSIS_X_CHANNEL 1
//the channel num of controlling horizontal speed
//左右的遥控器通道号码
#define CHASSIS_Y_CHANNEL 0

//in some mode, can use remote control to control rotation speed
//在特殊模式下，可以通过遥控器控制旋转
#define CHASSIS_YAW_CHANNEL 2
#define CHASSIS_WZ_CHANNEL 4

//the channel of choosing chassis mode,
//选择底盘状态 开关通道号
#define RADIO_CONTROL_SWITCH_R       0
#define RADIO_CONTROL_SWITCH_L       1
//rocker value (max 660) change to vertial speed (m/s) 
//遥控器前进摇杆（max 660）转化成车体前进速度（m/s）的比例
//#define CHASSIS_VX_RC_SEN 0.0032f
#define CHASSIS_VX_RC_SEN 0.9102f
//rocker value (max 660) change to horizontal speed (m/s)
//遥控器左右摇杆（max 660）转化成车体左右速度（m/s）的比例
//#define CHASSIS_VY_RC_SEN 0.0030f
#define CHASSIS_VY_RC_SEN 0.9102f
//in following yaw angle mode, rocker value add to angle 
//跟随底盘yaw模式下，遥控器的yaw遥杆（max 660）增加到车体角度的比例
#define CHASSIS_ANGLE_Z_RC_SEN 0.000002f
//in not following yaw angle mode, rocker value change to rotation speed
//不跟随云台的时候 遥控器的yaw遥杆（max 660）转化成车体旋转速度的比例
#define CHASSIS_WZ_RC_SEN 0.910225f

//#define CHASSIS_ACCEL_X_NUM 0.1666666667f
//#define CHASSIS_ACCEL_Y_NUM 0.3333333333f
//for_test
#define CHASSIS_ACCEL_X_NUM 0.49005f
#define CHASSIS_ACCEL_Y_NUM 0.49005f
#define CHASSIS_ACCEL_WZ_NUM 0.49005f
#define CHASSIS_FOLLOW_NORMAL_NUM 0.0f
#define CHASSIS_FOLLOW_SLOW_NUM 0.00005f
//rocker value deadline
//摇杆死区
#define CHASSIS_RC_DEADLINE 10

#define MOTOR_SPEED_TO_CHASSIS_SPEED_VX 0.25f
#define MOTOR_SPEED_TO_CHASSIS_SPEED_VY 0.25f
#define MOTOR_SPEED_TO_CHASSIS_SPEED_WZ 0.25f

//小陀螺速度
#define CHASSIS_SPIN_SPEED 4.0f


//#define MOTOR_DISTANCE_TO_CENTER 0.2f
#define MOTOR_DISTANCE_TO_CENTER 1.0f

//chassis task control time  1ms
//底盘任务控制间隔 1ms
#define CHASSIS_CONTROL_TIME_MS 1
//chassis task control time 0.001s
//底盘任务控制间隔 0.001s
#define CHASSIS_CONTROL_TIME 0.001f
//chassis control frequence, no use now.
//底盘任务控制频率，尚未使用这个宏
#define CHASSIS_CONTROL_FREQUENCE 1000.0f
//chassis 3508 max motor control current
//底盘3508最大can发送电流值
#define MAX_3508_MOTOR_CAN_CURRENT 16000.0f
//#define MAX_3508_MOTOR_CAN_CURRENT 8000.0f
//press the key, chassis will swing
//底盘摇摆按键
//#define SWING_KEY KEY_PRESSED_OFFSET_CTRL
//chassi forward, back, left, right key
//底盘前后左右控制按键
#define CHASSIS_FRONT_KEY KEY_PRESSED_OFFSET_W
#define CHASSIS_BACK_KEY KEY_PRESSED_OFFSET_S
#define CHASSIS_LEFT_KEY KEY_PRESSED_OFFSET_A
#define CHASSIS_RIGHT_KEY KEY_PRESSED_OFFSET_D
//#define CHASSIS_SPIN_LEFT_KEY KEY_PRESSED_OFFSET_Q
//#define CHASSIS_SPIN_RIGHT_KEY KEY_PRESSED_OFFSET_E
//m3508 rmp change to chassis speed,
//m3508转化成底盘速度(m/s)的比例，
#define M3508_MOTOR_19_RPM_TO_VECTOR 0.000415809748903494517209f
#define M3508_MOTOR_14_RPM_TO_VECTOR 0.000564313230654743f
#define CHASSIS_MOTOR_RPM_TO_VECTOR_SEN M3508_MOTOR_14_RPM_TO_VECTOR

//single chassis motor max speed
//单个底盘电机最大速度
#define MAX_WHEEL_SPEED 4.0f
//chassis forward or back max speed
//底盘运动过程最大前进速度
//#define NORMAL_MAX_CHASSIS_SPEED_X 4.0f
#define NORMAL_MAX_CHASSIS_SPEED_X 1.75f
//chassis left or right max speed
//底盘运动过程最大平移速度
//#define NORMAL_MAX_CHASSIS_SPEED_Y 3.0f
#define NORMAL_MAX_CHASSIS_SPEED_Y 1.75f

#define CHASSIS_WZ_SET_SCALE 0.5f

//when chassis is not set to move, swing max angle
//摇摆原地不动摇摆最大角度(rad)
#define SWING_NO_MOVE_ANGLE 0.7f
//when chassis is set to move, swing max angle
//摇摆过程底盘运动最大角度(rad)
#define SWING_MOVE_ANGLE 0.31415926535897932384626433832795f

//chassis motor speed PID
//底盘电机速度环PID
#define M3505_MOTOR_SPEED_PID_KP 8000.0f
#define M3505_MOTOR_SPEED_PID_KI 10.0f
#define M3505_MOTOR_SPEED_PID_KD 0.0f
#define M3505_MOTOR_SPEED_PID_MAX_OUT MAX_3508_MOTOR_CAN_CURRENT
#define M3505_MOTOR_SPEED_PID_MAX_IOUT 2000.0f

//chassis follow angle PID
//底盘旋转跟随PID
#define CHASSIS_FOLLOW_GIMBAL_PID_KP 2.0f
#define CHASSIS_FOLLOW_GIMBAL_PID_KI 0.0f
#define CHASSIS_FOLLOW_GIMBAL_PID_KD 0.0f
#define CHASSIS_FOLLOW_GIMBAL_PID_MAX_OUT 30.0f
#define CHASSIS_FOLLOW_GIMBAL_PID_MAX_IOUT 15.0f

typedef enum {
    CHASSIS_VECTOR_TO_GIMBAL_YAW,   //chassis will follow yaw gimbal motor relative angle.底盘会跟随云台相对角度
    CHASSIS_VECTOR_FOLLOW_GIMBAL_YAW,  //chassis will have yaw angle(chassis_yaw) close-looped control.底盘有底盘角度控制闭环
    CHASSIS_VECTOR_NO_FOLLOW_YAW,       //chassis will have rotation speed control. 底盘有旋转速度控制
    CHASSIS_VECTOR_RAW,                 //control-current will be sent to CAN bus derectly.
    CHASSIS_VECTOR_PID_AUTO_TUNE,
    CHASSIS_NOT_MOVE,

} chassis_mode_e;

typedef struct {
    const motor_measure_t *chassis_motor_measure;
    float32_t accel;
    float32_t speed;
    float32_t speed_set;
    int16_t give_current;
} chassis_motor_t;

typedef struct {
    const volatile RC_ctrl_t *chassis_RC;               //底盘使用的遥控器指针, the point to remote control
    const gimbal_motor_t *chassis_yaw_motor;   //will use the relative angle of yaw gimbal motor to calculate the euler angle.底盘使用到yaw云台电机的相对角度来计算底盘的欧拉角.
    const gimbal_motor_t *chassis_pitch_motor; //will use the relative angle of pitch gimbal motor to calculate the euler angle.底盘使用到pitch云台电机的相对角度来计算底盘的欧拉角
    const float32_t *chassis_INS_angle;             //the point to the euler angle of gyro sensor.获取陀螺仪解算出的欧拉角指针
    const volatile pid_auto_tune_t *pid_auto_tune_data_point;
    const volatile super_capacitance_measure_t *super_capacitance_measure_point;

    bool_t chassis_follow_reverse_flag;
    chassis_mode_e chassis_mode;               //state machine. 底盘控制状态机
    chassis_mode_e last_chassis_mode;          //last state machine.底盘上次控制状态机
    chassis_motor_t motor_chassis[4];          //chassis motor data.底盘电机数据
    pid_type_def motor_speed_pid[4];             //motor speed PID.底盘电机速度pid
    pid_type_def chassis_angle_pid;              //follow angle PID.底盘跟随角度pid
    pid_type_def chassis_vx_speed_pid;              //底盘速度闭环pid
    pid_type_def chassis_vy_speed_pid;              //底盘速度闭环pid
    pid_type_def chassis_wz_speed_pid;              //底盘速度闭环pid

    first_order_filter_type_t chassis_cmd_slow_set_vx;  //use first order filter to slow set-point.使用一阶低通滤波减缓设定值
    first_order_filter_type_t chassis_cmd_slow_set_vy;  //use first order filter to slow set-point.使用一阶低通滤波减缓设定值
    first_order_filter_type_t chassis_cmd_slow_set_wz;
    first_order_filter_type_t chassis_cmd_slow_yaw_follow;
    first_order_filter_type_t chassis_cmd_slow_spin;

    float32_t vx;                          //chassis vertical speed, positive means forward,unit m/s. 底盘速度 前进方向 前为正，单位 m/s
    float32_t vy;                          //chassis horizontal speed, positive means letf,unit m/s.底盘速度 左右方向 左为正  单位 m/s
    float32_t wz;                          //chassis rotation speed, positive means counterclockwise,unit rad/s.底盘旋转角速度，逆时针为正 单位 rad/s
    float32_t vx_set;                      //chassis set vertical speed,positive means forward,unit m/s.底盘设定速度 前进方向 前为正，单位 m/s
    float32_t vy_set;                      //chassis set horizontal speed,positive means left,unit m/s.底盘设定速度 左右方向 左为正，单位 m/s
    float32_t wz_set;                      //chassis set rotation speed,positive means counterclockwise,unit rad/s.底盘设定旋转角速度，逆时针为正 单位 rad/s
    float32_t chassis_relative_angle;      //the relative angle between chassis and gimbal.底盘与云台的相对角度，单位 rad
    float32_t chassis_relative_angle_set;  //the set relative angle.设置相对云台控制角度
    float32_t chassis_yaw_set;

    float32_t vx_max_speed;  //max forward speed, unit m/s.前进方向最大速度 单位m/s
    float32_t vx_min_speed;  //max backward speed, unit m/s.后退方向最大速度 单位m/s
    float32_t vy_max_speed;  //max letf speed, unit m/s.左方向最大速度 单位m/s
    float32_t vy_min_speed;  //max right speed, unit m/s.右方向最大速度 单位m/s
    float32_t chassis_yaw;   //the yaw angle calculated by gyro sensor and gimbal motor.陀螺仪和云台电机叠加的yaw角度
    float32_t chassis_pitch; //the pitch angle calculated by gyro sensor and gimbal motor.陀螺仪和云台电机叠加的pitch角度
    float32_t chassis_roll;  //the roll angle calculated by gyro sensor and gimbal motor.陀螺仪和云台电机叠加的roll角度

    uint16_t soft_power_limit;
    uint16_t power_limit;



} chassis_move_t;

extern chassis_move_t chassis_move;

/**
  * @brief          获取chassis_task栈大小
  * @param[in]      none
  * @retval         chassis_task_stack:任务堆栈大小
  */
extern uint32_t get_stack_of_chassis_task(void);

/**
  * @brief          chassis task, osDelay CHASSIS_CONTROL_TIME_MS (2ms) 
  * @param[in]      pvParameters: null
  * @retval         none
  */
/**
  * @brief          底盘任务，间隔 CHASSIS_CONTROL_TIME_MS 2ms
  * @param[in]      pvParameters: 空
  * @retval         none
  */
extern void chassis_task(void const *pvParameters);

/**
  * @brief          accroding to the channel value of remote control, calculate chassis vertical and horizontal speed set-point
  *                 
  * @param[out]     vx_set: vertical speed set-point
  * @param[out]     vy_set: horizontal speed set-point
  * @param[out]     chassis_move_rc_to_vector: "chassis_move" valiable point
  * @retval         none
  */
/**
  * @brief          根据遥控器通道值，计算纵向和横移速度
  *                 
  * @param[out]     vx_set: 纵向速度指针
  * @param[out]     vy_set: 横向速度指针
  * @param[out]     chassis_move_rc_to_vector: "chassis_move" 变量指针
  * @retval         none
  */
extern void chassis_rc_to_control_vector(float32_t *vx_set, float32_t *vy_set, float32_t *wz_set,
                                         chassis_move_t *chassis_move_rc_to_vector);

#endif
