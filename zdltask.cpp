#include "zdltask.h"


ec_master_t* EtherCATMaster::master = nullptr;
uint8_t* ZDLTask::domain1_pd = nullptr;

int ZDLTask::set_task_attribute(int cpu_affinity, int task_priority, int cycletime, int master_id){

    m_period_info.period_ns = cycletime;
    m_frequency = NSEC_PER_SEC/m_period_info.period_ns;
   
    master = ecrt_request_master(master_id);
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
    ecat::pdo_entry_idx idx,
    std::uint8_t *bit_pos,
    ecat::domain_index_type domain){

}


ZDLTask::ZDLTask(){
   
    
}

ZDLTask::~ZDLTask(){
    
}
