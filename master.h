#ifndef ETHERCAT_MASTER_H
#define ETHERCAT_MASTER_H


#include <cstdint>   // 用于 int32_t, uint16_t 等标准类型
#include <time.h>    // 用于 struct timespec
#include "ecrt.h"    // EtherCAT 主库接口


struct period_info {
    struct timespec next_period;
    long period_ns;
};



#define NSEC_PER_SEC 1000000000
#define FirstSlavePos  0, 0
#define TI5MOTOR 0x00522227, 0x00009253
#define MY_STACK_SIZE 8192


class EtherCATMaster {

public:
    EtherCATMaster();
    ~EtherCATMaster();
    // 初始化主站
    bool init_master();
};

private:



    // ---------- 静态成员变量 ----------
    static ec_master_t* master;                  // EtherCAT Master 对象
    static ec_master_state_t master_state;      // Master 状态

    static ec_domain_t* domain1;                // EtherCAT Domain
    static ec_domain_state_t domain1_state;    // Domain 状态

    static ec_slave_config_t* sc_1;             // 从站配置对象
    static ec_slave_config_state_t sc_1_state; // 从站状态

    static uint8_t* domain1_pd;                 // Domain1 的 process data 指针

    // PDO 偏移量
    static unsigned int off_status_word;
    static unsigned int off_control_word;
    static unsigned int off_mode;
    static unsigned int off_pos;
    static unsigned int off_mode_display;
    static unsigned int off_pos_actual;

    // PDO entry 注册表
    static const ec_pdo_entry_reg_t domain1_regs[];

    static unsigned int counter;
    static unsigned int blink;
    static unsigned int sync_ref_counter;

    // PDO 配置
    static ec_pdo_entry_info_t slave_0_pdo_entries[];
    static ec_pdo_info_t slave_0_pdos[];
    static ec_sync_info_t slave_0_syncs[];

    // DC 参数
    struct timespec wakeupTime, time;
    
    // 时间及周期结构体
    struct period_info {
        struct timespec next_period;
        long period_ns;
    };

    

    
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
    static void periodic_task_init(struct period_info *pinfo, long period);

    // 等待本周期剩余时间，使线程周期稳定
    static void wait_rest_of_period(struct period_info *pinfo);

    // 实时循环线程函数 
    void *simple_cyclic_task(void *data);

    // 配置实时线程参数并创建线程
    int config_rt_params_and_create_pthread();

    // 防止缺页异常导致延迟
    void stack_prefault(void);

    
#endif