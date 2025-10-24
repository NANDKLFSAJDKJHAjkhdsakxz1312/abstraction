#ifndef ZDLTASK_H
#define ZDLTASK_H
#include "ecrt.h"  
#include <time.h>  
#include <pthread.h>
#include <stdio.h>
#include <sys/mman.h> /* mlockall() */
#include <sched.h> /* sched_setscheduler() */
#include <stdlib.h> 
#include <string.h>
#include <functional>
#include <cstdint> 
#define MY_STACK_SIZE 8192
#define NSEC_PER_SEC 1000000000

#ifndef CLOCK_TO_USE
#define CLOCK_TO_USE CLOCK_MONOTONIC
#endif

#ifndef TIMESPEC2NS
#define TIMESPEC2NS(ts) ( (uint64_t)(ts).tv_sec * 1000000000ULL + (uint64_t)(ts).tv_nsec )
#endif



using PDOCallback = std::function<void()>;


class ZDLTask{
public:
    ZDLTask(int master_index);
    ~ZDLTask();
    int set_task_attribute(int cpu_affinity, int task_priority, int cycletime, int master_id);
    int start();
    void set_cycle_callback(PDOCallback cb){pdo_callback = cb;}
    void set_send_callback(PDOCallback cb){send_callback = cb;}
    void set_receive_callback(PDOCallback cb){receive_callback = cb;}
    void set_config_callback(PDOCallback cb){config_callback = cb;}
    template <typename T>
    bool try_register_pdo_entry(T *&addr, std::uint16_t slave_pos,
                              ecat::pdo_entry_idx idx);
private:

    void stack_prefault();
    // 实时循环线程函数 
    static void *simple_cyclic_task(void *data);
    // 初始化周期信息，用户设定周期(纳秒为单位)
    
    struct period_info {
        struct timespec next_period;
        long period_ns;
    };
    static void periodic_task_init(period_info *pinfo);
    // 实时任务
    void do_rt_task();
    // 等待本周期剩余时间，使线程周期稳定
    static void wait_rest_of_period(struct period_info *pinfo);
    // 计算下一个周期时间
    static void inc_period(struct period_info *pinfo);
    period_info m_period_info;
    pthread_attr_t m_attr;
    
    PDOCallback pdo_callback;
    PDOCallback send_callback;
    PDOCallback receive_callback;
    PDOCallback config_callback;
    static unsigned int counter;
    static unsigned int blink;
    static unsigned int sync_ref_counter;

    static ec_master_t* master;                  // EtherCAT Master 对象
    static ec_master_state_t master_state;      // Master 状态

    static ec_domain_t* domain1;                // EtherCAT Domain
    static ec_domain_state_t domain1_state;    // Domain 状态
    static uint8_t* domain1_pd;
 
    long m_frequency;
    // DC 参数
    struct timespec time;
};

#endif 
