#include "zdltask.h"

typedef enum {
    EC_DIR_INVALID,
    EC_DIR_OUTPUT,
    EC_DIR_INPUT,
    EC_DIR_COUNT
} ec_direction_t;

typedef enum {
    EC_WD_DEFAULT, /**< Use the default setting of the sync manager. */
    EC_WD_ENABLE, /**< Enable the watchdog. */
    EC_WD_DISABLE, /**< Disable the watchdog. */
} ec_watchdog_mode_t;

ec_direction_t direction_from_string(const std::string &dir_str) {
    if (dir_str == "EC_DIR_OUTPUT") {
        return EC_DIR_OUTPUT;
    } else if (dir_str == "EC_DIR_INPUT") {
        return EC_DIR_INPUT;
    } else if (dir_str == "EC_DIR_INVALID") {
        return EC_DIR_INVALID;
    } else {
        std::cerr << "Invalid direction string: " << dir_str << std::endl;
        return EC_DIR_INVALID;
    }
}

ec_watchdog_mode_t watchdog_from_string(cosnt std::string &dog_str){
    if(dog_str=="EC_WD_DEFAULT"){
        return EC_WD_DEFAULT;
    }else if(dog_str=="EC_WD_ENABLE"){
        return EC_WD_ENABLE;
    }else{
        return EC_WD_DISABLE;
    }
}

ec_master_t* EtherCATMaster::master = nullptr;
uint8_t* ZDLTask::domain1_pd = nullptr;

int ZDLTask::set_task_attribute(int cpu_affinity, int task_priority, int cycletime, int master_id){

    m_period_info.period_ns = cycletime;
    m_frequency = NSEC_PER_SEC/m_period_info.period_ns;
    ec_slave_config_t *sc;
    master = ecrt_request_master(master_id);
    if (!master) {
        return false;
    }
    domain1 = ecrt_master_create_domain(master);
    if (!domain1) {
        return false;
    }
    
    // for(SlaveStruct slave : slaves){
    //     if (!(sc = ecrt_master_slave_config(
    //                 master, slave.alias,slave.pos, slave.vendor_id,slave.product_code))) {
    //     fprintf(stderr, "Failed to get slave configuration.\n");
    //     return false;
    //     }
    //     if(slave.motor){
    //          //todo 通过pos来同一个sc建立对应的syncs
    //         YAML::Node config = YAML::LoadFile("./slaves.yaml");
    //         std::vector<ec_pdo_entry_info_t> pdo_entry;  // PDO 条目信息
    //         std::vector<ec_pdo_info_t> pdo;              // PDO 信息
    //         std::vector<ec_sync_info_t> sync; 
    //         int i = 0;
    //         for (auto other : config["others"]) {
    //             // 如果 pos 匹配
    //             if (other["pos"].as<int>() == slave.pos) {  // 假设 slave.pos = 1
    //                 // 填充 PDO 条目信息
    //                 for (auto pdo_entry_yaml : other["pdo_entries"]) {
    //                     ec_pdo_entry_info_t entry;
    //                     entry.index = pdo_entry_yaml["index"].as<int>();
    //                     entry.subindex = pdo_entry_yaml["subindex"].as<int>();
    //                     entry.bit_length = pdo_entry_yaml["bit_length"].as<int>();
    //                     pdo_entry.push_back(entry);
    //                 }

    //                 // 填充 PDO 信息
    //                 for (auto pdo_yaml : other["pdos"]) {
    //                     ec_pdo_info_t pdo_item;
    //                     pdo_item.index = pdo_yaml["index"].as<int>();
    //                     pdo_item.n_entries = pdo_yaml["num_entries"].as<int>();

    //                     // 通过 entries_ptr 选择相应的条目
    //                     int entries_ptr = pdo_yaml["entries_ptr"].as<int>();
                        
    //                     pdo_item.entries.push_back(pdo_entry+entries_ptr);
                        

    //                     pdo.push_back(pdo_item);
    //                 }

    //                 // 填充 Sync 信息
    //                 for (auto sync_yaml : other["syncs"]) {
    //                     ec_sync_info_t sync_item;
    //                     sync_item.index = sync_yaml["index"].as<int>();
    //                     sync_item.dir = direction_from_string(sync_yaml["direction"].as<std::string>());
    //                     sync_item.n_pdos = sync_yaml["num_pdos"].as<int>();

    //                     // 通过 pdos_ptr 选择相应的 PDO 信息
    //                     int pdos_ptr = sync_yaml["pdos_ptr"].as<int>();
                        
    //                     sync_item.pdos.push_back(pdo+pdos_ptr);
                        

    //                     sync_item.watchdog_mode = watchdog_from_string(sync_yaml["watchdog_mode"].as<std::string>());
    //                     sync.push_back(sync_item);
    //                 }
    //                 sync.push_back({0xff});
    //             }
    //         }
            
            

    //         if (ecrt_slave_config_pdos(sc, EC_END, sync)) {
    //             printf("Configuring PDOs...\n");
    //             fprintf(stderr, "Failed to configure PDOs.\n");
    //             return false;
    //         }
    //     }

         
        
    // }

    struct sched_param param;
    cpu_set_t cpuset;
    int ret;

    /*设置亲和性*/
    cpu_set_t cpuset;
  
    CPU_ZERO(&cpuset);

    CPU_SET(cpu_affinity, &cpuset); // 绑定 CPU

    ret = pthread_attr_init(&m_attr);
    if (ret) {
        printf("init pthread attributes failed\n");
        goto out;
    }

    pthread_attr_setaffinity_np(&m_attr, sizeof(cpu_set_t), &cpuset);
  
    /* 锁内存 */
    if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
        printf("mlockall() failed: %m\n");
        exit(-2);
    }
  
    stack_prefault();
    
    
  
    /* 设置 stack size  */
    ret = pthread_attr_setstacksize(&m_attr, PTHREAD_STACK_MIN + MY_STACK_SIZE);
    if (ret) {
        printf("pthread setstacksize failed\n");
        goto out;
    }
  
    /* 设置调度器策略和线程优先级  */
    ret = pthread_attr_setschedpolicy(&m_attr, SCHED_FIFO);
    if (ret) {
        printf("pthread setschedpolicy failed\n");
        goto out;
    }
   
    param.sched_priority = task_priority;
    ret = pthread_attr_setschedparam(&m_attr, &param);
    if (ret) {
        printf("pthread setschedparam failed\n");
        goto out;
    }
   
    ret = pthread_attr_setinheritsched(&m_attr, PTHREAD_EXPLICIT_SCHED);
    if (ret) {
        printf("pthread setinheritsched failed\n");
        goto out;
    }

    out:
    return ret;
}

int ZDLTask::start(){
    
    if(config_callback){
        printf("Registering PDO...\n");
        config_callback();
    }
    
    printf("Activating master...\n");
    if (ecrt_master_activate(master)) {
        return -1;
    }
    if (!(domain1_pd = ecrt_domain_data(domain1))) {
        return -1;
    }
    perform_delayed_binding();
    // for (auto const& [key, info] : binding_map) {
        
    //     // 1. 获取目标指针的指针 (void**)
    //     // info.target_addr_ptr 存储着外部 T* 变量的地址
    //     void **target_addr_ptr = info.target_addr_ptr; 
        
    //     // 2. 计算最终地址
    //     // 将 domain1_pd 视为 std::uint8_t* (或 char*)，并加上偏移量
    //     void *pdo_data_ptr = (std::uint8_t*)domain1_pd + info.offset; 
        
    //     // 3. 赋值给外部变量
    //     // 这一步将 pdo_data_ptr (void*) 的值赋给了外部 T* 变量
    //     *target_addr_ptr = pdo_data_ptr;
        
    //     // 可选：打印信息进行调试
    //     printf("Bound PDO (Slave %u, Index 0x%X) to address: %p\n", 
    //            key.first, key.second.index, pdo_data_ptr);
    // }
    /* Create a pthread with specified attributes */
    pthread_t thread;
    int ret;
    ret = pthread_create(&thread, &m_attr, simple_cyclic_task, this);
    if (ret) {
        printf("create pthread failed: %s\n", strerror(ret));
        goto out;
    }
 
    ret = pthread_setname_np(thread, "cyclic-rt");
    if (ret) {
        printf("failed to set thread name\n");
    }
   
    
  
out:
    return ret;
}

void ZDLTask::stack_prefault()
{
    unsigned char dummy[MY_STACK_SIZE];

    memset(dummy, 0, MY_STACK_SIZE);
}


void* ZDLTask::simple_cyclic_task(void *data)
{
    int cpu = sched_getcpu();
    printf("[RT] 实时线程启动，当前运行在 CPU 核心: %d\n", cpu);
    ZDLTask* self = static_cast<ZDLTask*>(data);
    ZDLTask::periodic_task_init(&self->m_period_info);

    clock_gettime(CLOCK_MONOTONIC, &self->m_period_info.next_period);
    self->m_period_info.next_period.tv_sec += 1; /* start in future */
    self->m_period_info.next_period.tv_nsec = 0;

    while (1) {
        
        wait_rest_of_period(&self->m_period_info);
      
        ecrt_master_application_time(master, TIMESPEC2NS(self->m_period_info.next_period));
      
        self->do_rt_task();   
        
    }

    return NULL;
}

void ZDLTask::periodic_task_init(period_info *pinfo) {
    clock_gettime(CLOCK_MONOTONIC, &(pinfo->next_period));
}


void ZDLTask::do_rt_task(){
    


// receive process data
  
    ecrt_master_receive(master);
    ecrt_domain_process(domain1);

    // check process data state
    check_domain1_state();

    if (counter) {
        counter--;
    } else { // do this at 1 Hz
        counter = m_frequency;
        
        blink = !blink;
       
        check_master_state();
      
        // check for slave configuration state(s) (optional)
        check_slave_config_states();
    }
    // exchange PDO
    

    if(receive_callback){
        receive_callback();
    }
    if(pdo_callback){
        pdo_callback();
    }
    if(send_callback){
        send_callback();
    }
    // sync every cycle
    if (sync_ref_counter) {
        sync_ref_counter--;
        
    } else {
        sync_ref_counter = 1; 
        
        clock_gettime(CLOCK_TO_USE, &time);
    
        ecrt_master_sync_reference_clock_to(master, TIMESPEC2NS(time));
        
    }
    
    ecrt_master_sync_slave_clocks(master);
    // send process data
    ecrt_domain_queue(domain1);
    ecrt_master_send(master);
   
}

void ZDLTask::wait_rest_of_period(struct period_info *pinfo)
{
    inc_period(pinfo);

    /* for simplicity, ignoring possibilities of signal wakes */
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,
            &pinfo->next_period, NULL);
}

void ZDLTask::inc_period(struct period_info *pinfo)
{
    pinfo->next_period.tv_nsec += pinfo->period_ns;

    while (pinfo->next_period.tv_nsec >= 1000000000) {
        /* timespec nsec overflow */
        pinfo->next_period.tv_sec++;
        pinfo->next_period.tv_nsec -= 1000000000;
    }
}

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


ZDLTask::ZDLTask(){
    ec_slave_config_t* ZDLTask::sc_0 = nullptr;
    ec_slave_config_state_t ZDLTask::sc_0_state = {};
    YAML::Node config = YAML::LoadFile("./slaves.yaml");
    for(YAML::Node slave_node : config["slaves"]){
        SlaveStruct slave;
        slave.alias = slave_node["alias"].as<int>();
        slave.pos = slave_node["pos"].as<int>();
        slave.motor = slave_node["motor"].as<bool>();
        slave.product_code = slave_node["product_code"].as<int>();
        slave.vendor_id = slave_node["vendor_id"].as<int>();
 
        slaves.push_back(slave);
    }
    
    

    
}

ZDLTask::~ZDLTask(){
    
}

void ZDLTask::perform_delayed_binding() {
    
    // 遍历所有需要延迟绑定的 PDO 条目
    for (auto const& [key, info] : binding_map) {
        
        
        if (!domain1_pd) {
            // 域数据尚未分配，可能 Master 激活失败
            continue;
        }

        // 2. 计算最终地址
        void *pdo_data_ptr = (std::uint8_t*)domain1_pd + info.offset; 
        
        // 3. 赋值给外部变量（通过存储的指针的指针）
        // info.target_addr_ptr 存储的是外部 T* 变量的地址。
        // *info.target_addr_ptr 相当于外部的 T* 变量本身。
        *info.target_addr_ptr = pdo_data_ptr; 
        
        // 此时，外部调用者传入的那个 'addr' 变量的值就被设置成了正确的 PDO 地址。
    }
}