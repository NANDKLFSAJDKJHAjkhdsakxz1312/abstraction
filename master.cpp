#include "master.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h> /* clock_gettime() */
#include <sys/mman.h> /* mlockall() */
#include <sched.h> /* sched_setscheduler() */
#include <limits.h>
#include <pthread.h>
#include <stdlib.h>





// 静态成员初始化


ec_master_t* EtherCATMaster::master = nullptr;
ec_master_state_t EtherCATMaster::master_state = {};

ec_domain_t* EtherCATMaster::domain1 = nullptr;
ec_domain_state_t EtherCATMaster::domain1_state = {};

ec_slave_config_t* EtherCATMaster::sc_0 = nullptr;
ec_slave_config_state_t EtherCATMaster::sc_0_state = {};

// ec_slave_config_t* EtherCATMaster::sc_1 = nullptr;
// ec_slave_config_state_t EtherCATMaster::sc_1_state = {};

// ec_slave_config_t* EtherCATMaster::sc_2 = nullptr;
// ec_slave_config_state_t EtherCATMaster::sc_2_state = {};


uint8_t* EtherCATMaster::domain1_pd = nullptr;

unsigned int EtherCATMaster::off_status_word_0 = 0;
unsigned int EtherCATMaster::off_control_word_0 = 0;
unsigned int EtherCATMaster::off_mode_0 = 0;
unsigned int EtherCATMaster::off_pos_0 = 0;
unsigned int EtherCATMaster::off_mode_display_0 = 0;
unsigned int EtherCATMaster::off_pos_actual_0 = 0;

// unsigned int EtherCATMaster::off_status_word_1 = 0;
// unsigned int EtherCATMaster::off_control_word_1 = 0;
// unsigned int EtherCATMaster::off_mode_1 = 0;
// unsigned int EtherCATMaster::off_pos_1 = 0;
// unsigned int EtherCATMaster::off_mode_display_1 = 0;
// unsigned int EtherCATMaster::off_pos_actual_1 = 0;

// unsigned int EtherCATMaster::off_status_word_2 = 0;
// unsigned int EtherCATMaster::off_control_word_2= 0;
// unsigned int EtherCATMaster::off_mode_2 = 0;
// unsigned int EtherCATMaster::off_pos_2 = 0;
// unsigned int EtherCATMaster::off_mode_display_2 = 0;
// unsigned int EtherCATMaster::off_pos_actual_2 = 0;
unsigned int EtherCATMaster::off_error_code = 0;

const ec_pdo_entry_reg_t EtherCATMaster::domain1_regs[] = {
    {FirstSlavePos,  ZDLRobot, 0x6040, 0, &off_control_word_0},
    {FirstSlavePos,  ZDLRobot, 0x607A, 0, &off_pos_0},
    {FirstSlavePos,  ZDLRobot, 0x6041, 0, &off_status_word_0},
    {FirstSlavePos,  ZDLRobot, 0x6060, 0, &off_mode_0},
    {FirstSlavePos,  ZDLRobot, 0x6061, 0, &off_mode_display_0},
    {FirstSlavePos,  ZDLRobot, 0x6064, 0, &off_pos_actual_0},
    {FirstSlavePos,  ZDLRobot, 0x603f, 0, &off_error_code},
    // {SecondSlavePos,  TI5MOTOR, 0x6040, 0, &off_control_word_0},
    // {SecondSlavePos,  TI5MOTOR, 0x607A, 0, &off_pos_0},
    // {SecondSlavePos,  TI5MOTOR, 0x6041, 0, &off_status_word_0},
    // {SecondSlavePos,  TI5MOTOR, 0x6060, 0, &off_mode_0},
    // {SecondSlavePos,  TI5MOTOR, 0x6061, 0, &off_mode_display_0},
    // {SecondSlavePos,  TI5MOTOR, 0x6064, 0, &off_pos_actual_0},
    // {ThirdSlavePos,  TI5MOTOR, 0x6040, 0, &off_control_word_1},
    // {ThirdSlavePos,  TI5MOTOR, 0x607A, 0, &off_pos_1},
    // {ThirdSlavePos,  TI5MOTOR, 0x6041, 0, &off_status_word_1},
    // {ThirdSlavePos,  TI5MOTOR, 0x6060, 0, &off_mode_1},
    // {ThirdSlavePos,  TI5MOTOR, 0x6061, 0, &off_mode_display_1},
    // {ThirdSlavePos,  TI5MOTOR, 0x6064, 0, &off_pos_actual_1},
    {}
};

unsigned int EtherCATMaster::counter = 0;
unsigned int EtherCATMaster::blink = 0;
unsigned int EtherCATMaster::sync_ref_counter = 0;


ec_pdo_entry_info_t EtherCATMaster::slave_0_pdo_entries[] = {
    {0x6040, 0x00, 16}, /* Control_Word */
    {0x6060, 0x00, 8}, /* Modes_of_Operation */
    {0x0000, 0x00, 8},
    {0x60ff, 0x00, 32}, /* Target_Velocity */
    {0x607a, 0x00, 32}, /* Target_Position */
    {0x6071, 0x00, 16}, /* Target_torque */
    {0x6041, 0x00, 16}, /* Status_Word */
    {0x603f, 0x00, 16}, /* Error_Code */
    {0x6061, 0x00, 8}, /* Modes_of_operation_display */
    {0x0000, 0x00, 8},
    {0x6064, 0x00, 32}, /* Position_actual_value */
    {0x606c, 0x00, 32}, /* Velocity_actual_value */
    {0x6077, 0x00, 16}, /* Torque_actual_value */
};

ec_pdo_info_t EtherCATMaster::slave_0_pdos[] = {
    {0x1600, 6, slave_0_pdo_entries + 0}, /* RxPDO201 */
    {0x1a00, 4, slave_0_pdo_entries + 6}, /* TxPDO181 */
    {0x1a01, 3, slave_0_pdo_entries + 10}, /* TxPDO281 */
};

ec_sync_info_t EtherCATMaster::slave_0_syncs[] = {
    {0, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE},
    {1, EC_DIR_INPUT, 0, NULL, EC_WD_DISABLE},
    {2, EC_DIR_OUTPUT, 1, slave_0_pdos + 0, EC_WD_ENABLE},
    {3, EC_DIR_INPUT, 2, slave_0_pdos + 1, EC_WD_DISABLE},
    {0xff}
};

/* Master 0, Slave 1, "Ti5Robot_JointMotor"
 * Vendor ID:       0x00522227
 * Product code:    0x00009253
 * Revision number: 0x00010005
 */

// ec_pdo_entry_info_t EtherCATMaster::slave_1_pdo_entries[] = {
//     {0x6040, 0x00, 16}, /* Control Word */
//     {0x607a, 0x00, 32}, /* Target Position */
//     {0x60ff, 0x00, 32}, /* Target Velocity */
//     {0x6071, 0x00, 16}, /* Target Torque */
//     {0x6060, 0x00, 8}, /* Modes of Operation */
//     {0x0000, 0x00, 8}, /* Gap */
//     {0x6041, 0x00, 16}, /* Status Word */
//     {0x6064, 0x00, 32}, /* Position Actual Value */
//     {0x606c, 0x00, 32}, /* Velocity Actual Value */
//     {0x6077, 0x00, 16}, /* Torque Actual Value */
//     {0x6061, 0x00, 8}, /* Modes of Operation Display */
//     {0x0000, 0x00, 8}, /* Gap */
// };

// ec_pdo_info_t EtherCATMaster::slave_1_pdos[] = {
//     {0x1600, 6, slave_1_pdo_entries + 0}, /* csp/csv RxPDO */
//     {0x1a00, 6, slave_1_pdo_entries + 6}, /* csp/csv TxPDO */
// };

// ec_sync_info_t EtherCATMaster::slave_1_syncs[] = {
//     {0, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE},
//     {1, EC_DIR_INPUT, 0, NULL, EC_WD_DISABLE},
//     {2, EC_DIR_OUTPUT, 1, slave_1_pdos + 0, EC_WD_ENABLE},
//     {3, EC_DIR_INPUT, 1, slave_1_pdos + 1, EC_WD_DISABLE},
//     {0xff}
// };

// /* Master 0, Slave 2, "Ti5Robot_JointMotor"
//  * Vendor ID:       0x00522227
//  * Product code:    0x00009253
//  * Revision number: 0x00010005
//  */

// ec_pdo_entry_info_t EtherCATMaster::slave_2_pdo_entries[] = {
//     {0x6040, 0x00, 16}, /* Control Word */
//     {0x607a, 0x00, 32}, /* Target Position */
//     {0x60ff, 0x00, 32}, /* Target Velocity */
//     {0x6071, 0x00, 16}, /* Target Torque */
//     {0x6060, 0x00, 8}, /* Modes of Operation */
//     {0x0000, 0x00, 8}, /* Gap */
//     {0x6041, 0x00, 16}, /* Status Word */
//     {0x6064, 0x00, 32}, /* Position Actual Value */
//     {0x606c, 0x00, 32}, /* Velocity Actual Value */
//     {0x6077, 0x00, 16}, /* Torque Actual Value */
//     {0x6061, 0x00, 8}, /* Modes of Operation Display */
//     {0x0000, 0x00, 8}, /* Gap */
// };

// ec_pdo_info_t EtherCATMaster::slave_2_pdos[] = {
//     {0x1600, 6, slave_2_pdo_entries + 0}, /* csp/csv RxPDO */
//     {0x1a00, 6, slave_2_pdo_entries + 6}, /* csp/csv TxPDO */
// };

// ec_sync_info_t EtherCATMaster::slave_2_syncs[] = {
//     {0, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE},
//     {1, EC_DIR_INPUT, 0, NULL, EC_WD_DISABLE},
//     {2, EC_DIR_OUTPUT, 1, slave_2_pdos + 0, EC_WD_ENABLE},
//     {3, EC_DIR_INPUT, 1, slave_2_pdos + 1, EC_WD_DISABLE},
//     {0xff}
// };

     
EtherCATMaster::period_info EtherCATMaster::pinfo = { {}, 1000000 };
long EtherCATMaster::frequency = NSEC_PER_SEC/pinfo.period_ns;

EtherCATMaster::EtherCATMaster() { }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             

EtherCATMaster::~EtherCATMaster() { } 



void EtherCATMaster::check_domain1_state(void)
{
    ec_domain_state_t ds;

    ecrt_domain_state(domain1, &ds);

    if (ds.working_counter != domain1_state.working_counter) {
        printf("Domain1: WC %u.\n", ds.working_counter);
    }
    if (ds.wc_state != domain1_state.wc_state) {
        printf("Domain1: State %u.\n", ds.wc_state);
    }

    domain1_state = ds;
}


void EtherCATMaster::check_master_state(void)
{
    ec_master_state_t ms;

    ecrt_master_state(master, &ms);

    if (ms.slaves_responding != master_state.slaves_responding) {
        printf("%u slave(s).\n", ms.slaves_responding);
    }
    if (ms.al_states != master_state.al_states) {
        printf("AL states: 0x%02X.\n", ms.al_states);
    }
    if (ms.link_up != master_state.link_up) {
        printf("Link is %s.\n", ms.link_up ? "up" : "down");
    }

    master_state = ms;
}


void EtherCATMaster::check_slave_config_states(void)
{
    // ---------------- 从站 0 状态 ----------------
    ec_slave_config_state_t s;

    ecrt_slave_config_state(sc_0, &s);

    if (s.al_state != sc_0_state.al_state) {
        printf("AnaIn: State 0x%02X.\n", s.al_state);
    }

    if (s.online != sc_0_state.online) {
        printf("AnaIn: %s.\n", s.online ? "online" : "offline");
    }
 
    if (s.operational != sc_0_state.operational) {
     
        printf("AnaIn: %soperational.\n", s.operational ? "" : "Not ");
    }

    sc_0_state = s;

    // ---------------- 从站 1 状态 ----------------
    // ecrt_slave_config_state(sc_1, &s);

    // if (s.al_state != sc_1_state.al_state) {
    //     printf("Slave1: State 0x%02X.\n", s.al_state);
    // }

    // if (s.online != sc_1_state.online) {
    //     printf("Slave1: %s.\n", s.online ? "online" : "offline");
    // }

    // if (s.operational != sc_1_state.operational) {
    //     printf("Slave1: %soperational.\n", s.operational ? "" : "Not ");
    // }

    // sc_1_state = s;
    // // ---------------- 从站 2 状态 ----------------
    // ecrt_slave_config_state(sc_2, &s);

    // if (s.al_state != sc_2_state.al_state) {
    //     printf("Slave1: State 0x%02X.\n", s.al_state);
    // }

    // if (s.online != sc_2_state.online) {
    //     printf("Slave1: %s.\n", s.online ? "online" : "offline");
    // }

    // if (s.operational != sc_2_state.operational) {
    //     printf("Slave1: %soperational.\n", s.operational ? "" : "Not ");
    // }

    // sc_2_state = s;
}


void EtherCATMaster::do_rt_task(){
    
    // receive process data
  
    ecrt_master_receive(master);
    ecrt_domain_process(domain1);

    // check process data state
    check_domain1_state();

    if (counter) {
        counter--;
    } else { // do this at 1 Hz
        counter = frequency;
        printf("status_0: 0x%04x,mode_0: 0x%04x, mode_disp_0:0x%04x, actual_pos_0:%d\n", status_0,mode_0,mode_disp_0,pos_actual_0); 
        // printf("status_1: 0x%04x,mode_1: 0x%04x, mode_disp_1:0x%04x, actual_pos_1:%d\n", status_1,mode_1,mode_disp_1,pos_actual_1); 
        // printf("status_2: 0x%04x,mode_2: 0x%04x, mode_disp_2:0x%04x, actual_pos_2:%d,ERROR_CODE:%d\n", status_2,mode_2,mode_disp_2,pos_actual_2,error_code); 
        // calculate new process data
        blink = !blink;// exchange PDO
    
        status_0 = EC_READ_U16(domain1_pd + off_status_word_0);
        mode_0 = EC_READ_S8(domain1_pd + off_mode_0); 
        mode_disp_0 = EC_READ_S8(domain1_pd + off_mode_display_0);
        pos_actual_0 = EC_READ_S32(domain1_pd + off_pos_actual_0);
        int32_t pos_actual_1 = ((int32_t) *((uint32_t *)((void *) (domain1_pd + off_pos_actual_0))));
        printf("actual_pos_1:%d\n", pos_actual_1); 
        check_master_state();
      
        // check for slave configuration state(s) (optional)
        check_slave_config_states();
    }
        // exchange PDO
    
        status_0 = EC_READ_U16(domain1_pd + off_status_word_0);
        mode_0 = EC_READ_S8(domain1_pd + off_mode_0); 
        mode_disp_0 = EC_READ_S8(domain1_pd + off_mode_display_0);
        pos_actual_0 = EC_READ_S32(domain1_pd + off_pos_actual_0);

        // status_1 = EC_READ_U16(domain1_pd + off_status_word_1);
        // mode_1 = EC_READ_S8(domain1_pd + off_mode_1); 
        // mode_disp_1 = EC_READ_S8(domain1_pd + off_mode_display_1);
        // pos_actual_1 = EC_READ_S32(domain1_pd + off_pos_actual_1);

        // status_2 = EC_READ_U16(domain1_pd + off_status_word_2);
        // mode_2 = EC_READ_S8(domain1_pd + off_mode_2); 
        // mode_disp_2 = EC_READ_S8(domain1_pd + off_mode_display_2);
        // pos_actual_2 = EC_READ_S32(domain1_pd + off_pos_actual_2);
        // error_code = EC_READ_U16(domain1_pd+off_error_code);
        // printf("status_0: 0x%04x,mode_0: 0x%04x, mode_disp_0:0x%04x, actual_pos:%d\n", status_0,mode_0,mode_disp_0,pos_actual_0); 
        if(cw_flag06){
            EC_WRITE_U16(domain1_pd+off_control_word_0,0x06);
            // EC_WRITE_U16(domain1_pd+off_control_word_1,0x06);
            // EC_WRITE_U16(domain1_pd+off_control_word_2,0x06);
            cw_flag06 = false;
        }
        if(cw_flag07){
            EC_WRITE_U16(domain1_pd+off_control_word_0,0x07);
            // EC_WRITE_U16(domain1_pd+off_control_word_1,0x07);
            // EC_WRITE_U16(domain1_pd+off_control_word_2,0x07);
            cw_flag07 =false;
        }
        if(cw_flag0f){
            EC_WRITE_U16(domain1_pd+off_control_word_0,0x0f);
            // EC_WRITE_U16(domain1_pd+off_control_word_1,0x0f);
            // EC_WRITE_U16(domain1_pd+off_control_word_2,0x0f);
            cw_flag0f =false;
        }
        if(cw_flag80){
            EC_WRITE_U16(domain1_pd+off_control_word_0,0x80);
            // EC_WRITE_U16(domain1_pd+off_control_word_1,0x80);
            // EC_WRITE_U16(domain1_pd+off_control_word_2,0x80);
            cw_flag80 =false;
        }
        if(mode_flag){
            EC_WRITE_S8(domain1_pd+off_mode_0,0X08);
            // EC_WRITE_S8(domain1_pd+off_mode_1,0X08);
            // EC_WRITE_S8(domain1_pd+off_mode_2,0X08);
            mode_flag =false;
        }
        if(pos_flag_0){
            EC_WRITE_S32(domain1_pd+off_pos_0,pos_value_0);
            
        }
        // if(pos_flag_1){
            
        //     EC_WRITE_S32(domain1_pd+off_pos_1,pos_value_1);
        // }
        // if(pos_flag_2){
            
        //     EC_WRITE_S32(domain1_pd+off_pos_2,pos_value_2);
        // }
        
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


void EtherCATMaster::inc_period(struct period_info *pinfo)
{
    pinfo->next_period.tv_nsec += pinfo->period_ns;

    while (pinfo->next_period.tv_nsec >= 1000000000) {
        /* timespec nsec overflow */
        pinfo->next_period.tv_sec++;
        pinfo->next_period.tv_nsec -= 1000000000;
    }
}


void EtherCATMaster::periodic_task_init(struct period_info *pinfo)
{
    /* for simplicity, hardcoding a 1 ms period */
    pinfo->period_ns = 1000000;

    clock_gettime(CLOCK_MONOTONIC, &(pinfo->next_period));
}


void EtherCATMaster::wait_rest_of_period(struct period_info *pinfo)
{
    inc_period(pinfo);

    /* for simplicity, ignoring possibilities of signal wakes */
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,
            &pinfo->next_period, NULL);
}


void* EtherCATMaster::simple_cyclic_task(void *data)
{
    int cpu = sched_getcpu();
    printf("[RT] 实时线程启动，当前运行在 CPU 核心: %d\n", cpu);
    EtherCATMaster* self = static_cast<EtherCATMaster*>(data);
    
    periodic_task_init(&pinfo);

    clock_gettime(CLOCK_MONOTONIC, &pinfo.next_period);
    pinfo.next_period.tv_sec += 1; /* start in future */
    pinfo.next_period.tv_nsec = 0;

    while (1) {
        
        wait_rest_of_period(&pinfo);
      
        ecrt_master_application_time(master, TIMESPEC2NS(pinfo.next_period));
      
        self->do_rt_task();   
        
    }

    return NULL;
}


int EtherCATMaster::config_rt_params_and_create_pthread(){
    struct sched_param param;
    pthread_attr_t attr;
    pthread_t thread;
    int ret;

  
    /*设置亲和性*/
    cpu_set_t cpuset;
  
    CPU_ZERO(&cpuset);

    CPU_SET(2, &cpuset); // 绑定到 CPU1
 
    /* Initialize pthread attributes (default values) */
    ret = pthread_attr_init(&attr);
    if (ret) {
        printf("init pthread attributes failed\n");
        goto out;
    }

    pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpuset);
  
    /* Lock memory */
    if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
        printf("mlockall() failed: %m\n");
        exit(-2);
    }
  
    stack_prefault();
    
    
  
    /* Set a specific stack size  */
    ret = pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN + MY_STACK_SIZE);
    if (ret) {
        printf("pthread setstacksize failed\n");
        goto out;
    }
  
    /* Set scheduler policy and priority of pthread */
    ret = pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    if (ret) {
        printf("pthread setschedpolicy failed\n");
        goto out;
    }
   
    param.sched_priority = 98;
    ret = pthread_attr_setschedparam(&attr, &param);
    if (ret) {
        printf("pthread setschedparam failed\n");
        goto out;
    }
   
    /* Use scheduling parameters of attr */
    ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    if (ret) {
        printf("pthread setinheritsched failed\n");
        goto out;
    }
  
    /* Create a pthread with specified attributes */
    ret = pthread_create(&thread, &attr, simple_cyclic_task, this);
    if (ret) {
        printf("create pthread failed: %s\n", strerror(ret));
        goto out;
    }
 
    ret = pthread_setname_np(thread, "cyclic-rt-1-ms");
    if (ret) {
        printf("failed to set thread name\n");
    }
   
    
  
out:
    return ret;
}

void EtherCATMaster::stack_prefault(void)
{
    unsigned char dummy[MY_STACK_SIZE];

    memset(dummy, 0, MY_STACK_SIZE);
}


bool EtherCATMaster::init_master(){
    master = ecrt_request_master(0);

    if (!master) {
        return false;
    }

    domain1 = ecrt_master_create_domain(master);
    if (!domain1) {pdo_entries
        return false;
    }

    if (!(sc_0 = ecrt_master_slave_config(
                    master, FirstSlavePos, ZDLRobot))) {
        fprintf(stderr, "Failed to get slave configuration.\n");
        return false;
    }

    printf("Configuring PDOs...\n");
    if (ecrt_slave_config_pdos(sc_0, EC_END, slave_0_syncs)) {
        fprintf(stderr, "Failed to configure PDOs.\n");
        return false;
    }

    // if (!(sc_1 = ecrt_master_slave_config(
    //                 master, SecondSlavePos, TI5MOTOR))) {
    //     fprintf(stderr, "Failed to get slave configuration.\n");
    //     return false;
    // }

    // printf("Configuring PDOs...\n");
    // if (ecrt_slave_config_pdos(sc_1, EC_END, slave_1_syncs)) {
    //     fprintf(stderr, "Failed to configure PDOs.\n");
    //     return false;
    // }

    // if (!(sc_2 = ecrt_master_slave_config(
    //                 master, ThirdSlavePos, TI5MOTOR))) {
    //     fprintf(stderr, "Failed to get slave configuration.\n");
    //     return false;
    // }

    // printf("Configuring PDOs...\n");
    // if (ecrt_slave_config_pdos(sc_2, EC_END, slave_2_syncs)) {
    //     fprintf(stderr, "Failed to configure PDOs.\n");
    //     return false;
    // }

    

    if (ecrt_domain_reg_pdo_entry_list(domain1, domain1_regs)) {
        fprintf(stderr, "PDO entry registration failed!\n");
        return false;
    }
    printf("off_control_word_0=%u off_status_word_0=%u off_mode_0=%u off_pos_0=%u off_mode_display_0=%u\n",
       off_control_word_0, off_status_word_0, off_mode_0, off_pos_0, off_mode_display_0);
    // printf("off_control_word_1=%u off_status_word_1=%u off_mode_1=%u off_pos_1=%u off_mode_display_1=%u\n",
    //    off_control_word_1, off_status_word_1, off_mode_1, off_pos_1, off_mode_display_1);
    // printf("off_control_word_2=%u off_status_word_2=%u off_mode_2=%u off_pos_2=%u off_mode_display_2=%u\n",
    //    off_control_word_2, off_status_word_2, off_mode_2, off_pos_2, off_mode_display_2);

    ecrt_slave_config_dc(sc_0, 0x0300, pinfo.period_ns, 0, 0, 0);
    // ecrt_slave_config_dc(sc_1, 0x0300, pinfo.period_ns, 0, 0, 0);
    // ecrt_slave_config_dc(sc_2, 0x0300, pinfo.period_ns, 0, 0, 0);

    printf("Activating master...\n");
    if (ecrt_master_activate(master)) {
        return false;
    }
  
    if (!(domain1_pd = ecrt_domain_data(domain1))) {
        return false;
    }
  
    return true;
}


void EtherCATMaster::send_control_word06(uint16_t value){
    cw_flag06 = true;
    
}
void EtherCATMaster::send_control_word07(uint16_t value){
    cw_flag07 = true;
    
}
void EtherCATMaster::send_control_word0f(uint16_t value){
    cw_flag0f = true;
    
}
void EtherCATMaster::send_control_word80(uint16_t value){
    cw_flag80 = true;
    
}

void EtherCATMaster::send_target_pos_0(int32_t value){
    pos_flag_0 = true;
    pos_value_0 = value;
}

// void EtherCATMaster::send_target_pos_1(int32_t value){
//     pos_flag_1 = true;
//     pos_value_1 = value;
// }

// void EtherCATMaster::send_target_pos_2(int32_t value){
//     pos_flag_2 = true;
//     pos_value_2 = -182413000;
// }

void EtherCATMaster::send_mode_of_operation(int8_t value)
{
    mode_flag = true;
    
}