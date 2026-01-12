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
#include "slavestruct.h"
#include <yaml-cpp/yaml.h>

#define MY_STACK_SIZE 8192
#define NSEC_PER_SEC 1000000000

#ifndef CLOCK_TO_USE
#define CLOCK_TO_USE CLOCK_MONOTONIC
#endif

#ifndef TIMESPEC2NS
#define TIMESPEC2NS(ts) ( (uint64_t)(ts).tv_sec * 1000000000ULL + (uint64_t)(ts).tv_nsec )
#endif



using PDOCallback = std::function<void()>;

struct pdo_entry_idx{
        int16_t index;
        int16_t subindex;
    };

struct DelayedBindingInfo {
        unsigned int offset;
        void **target_addr_ptr; // 存储外部 T*&addr (即 addr 变量的地址)
     
    };



class ZDLTask{
public:
    
    ZDLTask();
    ~ZDLTask();
    void perform_delayed_binding();
    int set_task_attribute(int cpu_affinity, int task_priority, int cycletime, int master_id);
    int start();
    void set_cycle_callback(PDOCallback cb){pdo_callback = cb;}
    void set_send_callback(PDOCallback cb){send_callback = cb;}
    void set_receive_callback(PDOCallback cb){receive_callback = cb;}
    void set_config_callback(PDOCallback cb){config_callback = cb;}
    template <typename T>
    template <typename T>
    bool try_register_pdo_entry(T *&addr,
    std::uint16_t slave_pos,
    pdo_entry_idx idx,
    std::uint8_t *bit_pos,
    ec_domain_t *domain){

    ec_pdo_entry_reg_t domain_regs;
    YAML::Node config = YAML::LoadFile("./slaves.yaml");
    for(auto slave_node : config["slaves"]){
        if(slave_pos==slave_node["pos"]){
            if (!(sc_0 = ecrt_master_slave_config(
                master, slave_node["alias"].as<int>(),slave_node["pos"].as<int>(),slave_node["vendor_id"].as<int>(), slave_node["product_code"].as<int>()))) {
            fprintf(stderr, "Failed to get slave configuration.\n");
            return false;
        }
        if(slave_node["motor"].as<bool>()){
            
                
                std::vector<ec_pdo_entry_info_t> pdo_entry;  // PDO 条目信息
                std::vector<ec_pdo_info_t> pdo;              // PDO 信息
                std::vector<ec_sync_info_t> sync; 
                int i = 0;
                for (auto other : config["others"]) {
                    // 如果 pos 匹配
                    if (other["pos"].as<int>() == slave_pos) {  // 假设 slave.pos = 1
                        // 填充 PDO 条目信息
                        for (auto pdo_entry_yaml : other["pdo_entries"]) {
                            ec_pdo_entry_info_t entry;
                            entry.index = pdo_entry_yaml["index"].as<int>();
                            entry.subindex = pdo_entry_yaml["subindex"].as<int>();
                            entry.bit_length = pdo_entry_yaml["bit_length"].as<int>();
                            pdo_entry.push_back(entry);
                        }

                        // 填充 PDO 信息
                        for (auto pdo_yaml : other["pdos"]) {
                            ec_pdo_info_t pdo_item;
                            pdo_item.index = pdo_yaml["index"].as<int>();
                            pdo_item.n_entries = pdo_yaml["num_entries"].as<int>();

                            // 通过 entries_ptr 选择相应的条目
                            int entries_ptr = pdo_yaml["entries_ptr"].as<int>();
                            
                            pdo_item.entries.push_back(pdo_entry+entries_ptr);
                            

                            pdo.push_back(pdo_item);
                        }

                        // 填充 Sync 信息
                        for (auto sync_yaml : other["syncs"]) {
                            ec_sync_info_t sync_item;
                            sync_item.index = sync_yaml["index"].as<int>();
                            sync_item.dir = direction_from_string(sync_yaml["direction"].as<std::string>());
                            sync_item.n_pdos = sync_yaml["num_pdos"].as<int>();

                            // 通过 pdos_ptr 选择相应的 PDO 信息
                            int pdos_ptr = sync_yaml["pdos_ptr"].as<int>();
                            
                            sync_item.pdos.push_back(pdo+pdos_ptr);
                            

                            sync_item.watchdog_mode = watchdog_from_string(sync_yaml["watchdog_mode"].as<std::string>());
                            sync.push_back(sync_item);
                        }
                        sync.push_back({0xff});
                    }
                }
                
                

                if (ecrt_slave_config_pdos(sc_0, EC_END, sync)) {
                    printf("Configuring PDOs...\n");
                    fprintf(stderr, "Failed to configure PDOs.\n");
                    return false;
                }
            
        }
        ec_pdo_entry_reg_t pdo_entry_reg;
        pdo_entry_reg.alias = slave_node["alias"];
        pdo_entry_reg.position = slave_node["pos"];
        pdo_entry_reg.vendor_id = slave_node["vendor_id"];
        pdo_entry_reg.product_code = slave_node["product_code"];
        pdo_entry_reg.index = idx.index;
        pdo_entry_reg.subindex = idx.subindex;
        unsigned int offset = 0;
        pdo_entry_reg.offset = &offset;
        unsigned int ret = ecrt_slave_config_reg_pdo_entry(sc_0, pdo_entry_reg.index,
                    pdo_entry_reg.subindex, domain1);
        

        pdo_entry_reg.offset = ret;
        DelayedBindingInfo info;
        info.offset = ret;
        info.target_addr_ptr = (void**)&addr;
        binding_map[{slave_pos, idx}] = info; // 假设 pdo_entry_idx 可以作为 map key

        }
        
        
            
        
    }




    }


private:
    std::map<std::pair<std::uint16_t, pdo_entry_idx>, DelayedBindingInfo> binding_map;
    std::vector<SlaveStruct> slaves;
    static ec_slave_config_t* sc_0;             // 从站配置对象
    static ec_slave_config_state_t sc_0_state; // 从站状态
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
