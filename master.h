#ifndef ETHERCAT_MASTER_H
#define ETHERCAT_MASTER_H


#include <cstdint>   // 用于 int32_t, uint16_t 等标准类型
#include <time.h>    // 用于 struct timespec
#include "ecrt.h"    // EtherCAT 主库接口





#ifndef CLOCK_TO_USE
#define CLOCK_TO_USE CLOCK_MONOTONIC
#endif

#ifndef TIMESPEC2NS
#define TIMESPEC2NS(ts) ( (uint64_t)(ts).tv_sec * 1000000000ULL + (uint64_t)(ts).tv_nsec )
#endif



#define NSEC_PER_SEC 1000000000
#define FirstSlavePos  0, 0
#define SecondSlavePos  0, 1
#define ThirdSlavePos  0, 2
#define TI5MOTOR 0x00522227, 0x00009253
#define ZDLRobot 0x000004d8, 0x00002006
#define MY_STACK_SIZE 8192



class EtherCATMaster {

public:
    EtherCATMaster();
    ~EtherCATMaster();
    // 初始化主站
    bool init_master();
    // 配置实时线程参数并创建线程
    int config_rt_params_and_create_pthread();
        // 发送控制字接口
    void send_control_word06(uint16_t value);
    void send_control_word07(uint16_t value);
    void send_control_word0f(uint16_t value);
    void send_control_word80(uint16_t value);
    // 发送目标位置接口
    void send_target_pos_0(int32_t value );
    void send_target_pos_1(int32_t value );
    void send_target_pos_2(int32_t value );
    // 改变控制模式接口
    void send_mode_of_operation(int8_t value);
private:



    // ---------- 静态成员变量 ----------

    

    static ec_master_t* master;                  // EtherCAT Master 对象
    static ec_master_state_t master_state;      // Master 状态

    static ec_domain_t* domain1;                // EtherCAT Domain
    static ec_domain_state_t domain1_state;    // Domain 状态

    static ec_slave_config_t* sc_0;             // 从站配置对象
    static ec_slave_config_state_t sc_0_state; // 从站状态

    static ec_slave_config_t* sc_1;             // 从站配置对象
    static ec_slave_config_state_t sc_1_state; // 从站状态

    static ec_slave_config_t* sc_2;             // 从站配置对象
    static ec_slave_config_state_t sc_2_state; // 从站状态

    static uint8_t* domain1_pd;                 // Domain1 的 process data 指针

    // PDO 偏移量
    static unsigned int off_status_word_0;
    static unsigned int off_control_word_0;
    static unsigned int off_mode_0;
    static unsigned int off_pos_0;
    static unsigned int off_mode_display_0;
    static unsigned int off_pos_actual_0;

    // static unsigned int off_status_word_1;
    // static unsigned int off_control_word_1;
    // static unsigned int off_mode_1;
    // static unsigned int off_pos_1;
    // static unsigned int off_mode_display_1;
    // static unsigned int off_pos_actual_1;

    // static unsigned int off_status_word_2;
    // static unsigned int off_control_word_2;
    // static unsigned int off_mode_2;
    // static unsigned int off_pos_2;
    // static unsigned int off_mode_display_2;
    // static unsigned int off_pos_actual_2;
    static unsigned int off_error_code;

    // PDO entry 注册表
    static const ec_pdo_entry_reg_t domain1_regs[];

    static unsigned int counter;
    static unsigned int blink;
    static unsigned int sync_ref_counter;

    // PDO 配置
    static ec_pdo_entry_info_t slave_0_pdo_entries[];
    static ec_pdo_info_t slave_0_pdos[];
    static ec_sync_info_t slave_0_syncs[];

    static ec_pdo_entry_info_t slave_1_pdo_entries[];
    static ec_pdo_info_t slave_1_pdos[];
    static ec_sync_info_t slave_1_syncs[];

    static ec_pdo_entry_info_t slave_2_pdo_entries[];
    static ec_pdo_info_t slave_2_pdos[];
    static ec_sync_info_t slave_2_syncs[];

    // PDO 变量
    uint16_t status_0 = 0;
    int8_t mode_0= 0;
    int8_t mode_disp_0= 0;
    int32_t pos_actual_0= 0;

    // uint16_t status_1 = 0;
    // int8_t mode_1= 0;
    // int8_t mode_disp_1= 0;
    // int32_t pos_actual_1= 0;

    // uint16_t status_2 = 0;
    // int8_t mode_2= 0;
    // int8_t mode_disp_2= 0;
    // int32_t pos_actual_2= 0;
    // uint16_t error_code = 0;

    // DC 参数
    struct timespec time;
    
    // 时间及周期结构体
    struct period_info {
        struct timespec next_period;
        long period_ns;
    };

    static period_info pinfo;
    static long frequency;

    // 接口函数标志位
    bool cw_flag06 = false;   
    bool cw_flag07 = false;    
    bool cw_flag0f = false; 
    bool cw_flag80 = false;  
    bool mode_flag = false; 
    bool pos_flag_0 = false; 
    // bool pos_flag_1 = false;
    // bool pos_flag_2 = false;
    int32_t pos_value_0 = 0;
    // int32_t pos_value_1 = 0;
    // int32_t pos_value_2 = 0;
    // 检查 Domain 状态
    void check_domain1_state(void);

    // 检查 Master 状态
    void check_master_state(void);

    // 检查从站配置状态
    void check_slave_config_states(void);

    // 实时任务
    void do_rt_task(void);

    // 计算下一个周期时间
    static void inc_period(struct period_info *pinfo);

    // 初始化周期信息，用户设定周期(纳秒为单位)
    static void periodic_task_init(struct period_info *pinfo);

    // 等待本周期剩余时间，使线程周期稳定
    static void wait_rest_of_period(struct period_info *pinfo);

    // 实时循环线程函数 
    static void *simple_cyclic_task(void *data);

    // 防止缺页异常导致延迟
    void stack_prefault(void);

    


};
    
#endif