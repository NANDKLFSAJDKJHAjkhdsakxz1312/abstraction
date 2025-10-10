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

ec_slave_config_t* EtherCATMaster::sc_1 = nullptr;
ec_slave_config_state_t EtherCATMaster::sc_1_state = {};

uint8_t* EtherCATMaster::domain1_pd = nullptr;

unsigned int EtherCATMaster::off_status_word = 0;
unsigned int EtherCATMaster::off_control_word = 0;
unsigned int EtherCATMaster::off_mode = 0;
unsigned int EtherCATMaster::off_pos = 0;
unsigned int EtherCATMaster::off_mode_display = 0;
unsigned int EtherCATMaster::off_pos_actual = 0;

const ec_pdo_entry_reg_t EtherCATMaster::domain1_regs[] = {
    {FirstSlavePos,  TI5MOTOR, 0x6040, 0, &off_control_word},
    {FirstSlavePos,  TI5MOTOR, 0x607A, 0, &off_pos},
    {FirstSlavePos,  TI5MOTOR, 0x6041, 0, &off_status_word},
    {FirstSlavePos,  TI5MOTOR, 0x6060, 0, &off_mode},
    {FirstSlavePos,  TI5MOTOR, 0x6061, 0, &off_mode_display},
    {FirstSlavePos,  TI5MOTOR, 0x6064, 0, &off_pos_actual},
    {}
};

unsigned int EtherCATMaster::counter = 0;
unsigned int EtherCATMaster::blink = 0;
unsigned int EtherCATMaster::sync_ref_counter = 0;


ec_pdo_entry_info_t slave_0_pdo_entries[] = {
    {0x6040, 0x00, 16},
    {0x607a, 0x00, 32},
    {0x60ff, 0x00, 32},
    {0x6071, 0x00, 16},
    {0x6060, 0x00, 8},
    {0x0000, 0x00, 8}, /* Gap */
    {0x6041, 0x00, 16},
    {0x6064, 0x00, 32},
    {0x606c, 0x00, 32},
    {0x6077, 0x00, 16},
    {0x6061, 0x00, 8},
    {0x0000, 0x00, 8}, /* Gap */
};

ec_pdo_info_t slave_0_pdos[] = {
    {0x1600, 6, slave_0_pdo_entries + 0},
    {0x1a00, 6, slave_0_pdo_entries + 6},
};

ec_sync_info_t slave_0_syncs[] = {
    {0, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE},
    {1, EC_DIR_INPUT, 0, NULL, EC_WD_DISABLE},
    {2, EC_DIR_OUTPUT, 1, slave_0_pdos + 0, EC_WD_ENABLE},
    {3, EC_DIR_INPUT, 1, slave_0_pdos + 1, EC_WD_DISABLE},
    {0xff}
};


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
    ec_slave_config_state_t s;

    ecrt_slave_config_state(sc_ana_in, &s);

    if (s.al_state != sc_ana_in_state.al_state) {
        printf("AnaIn: State 0x%02X.\n", s.al_state);
    }
    if (s.online != sc_ana_in_state.online) {
        printf("AnaIn: %s.\n", s.online ? "online" : "offline");
    }
    if (s.operational != sc_ana_in_state.operational) {
        printf("AnaIn: %soperational.\n", s.operational ? "" : "Not ");
    }

    sc_ana_in_state = s;
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
        counter = FREQUENCY;

        // calculate new process data
        blink = !blink;

        // check for master state (optional)
        check_master_state();

        // check for slave configuration state(s) (optional)
        check_slave_config_states();

        // exchange process data

        // sync every cycle
        if (sync_ref_counter) {
            sync_ref_counter--;
        } else {
            sync_ref_counter = 1; 

            clock_gettime(CLOCK_TO_USE, time);
            ecrt_master_sync_reference_clock_to(master, TIMESPEC2NS(*time));
        }
        ecrt_master_sync_slave_clocks(master);
        // send process data
        ecrt_domain_queue(domain1);
        ecrt_master_send(master);
    }
}


static void EtherCATMaster::inc_period(struct period_info *pinfo)
{
    pinfo->next_period.tv_nsec += pinfo->period_ns;

    while (pinfo->next_period.tv_nsec >= 1000000000) {
        /* timespec nsec overflow */
        pinfo->next_period.tv_sec++;
        pinfo->next_period.tv_nsec -= 1000000000;
    }
}


static void EtherCATMaster::periodic_task_init(struct period_info *pinfo)
{
    /* for simplicity, hardcoding a 1 ms period */
    pinfo->period_ns = 1000000;

    clock_gettime(CLOCK_MONOTONIC, &(pinfo->next_period));
}


static void EtherCATMaster::wait_rest_of_period(struct period_info *pinfo)
{
    inc_period(pinfo);

    /* for simplicity, ignoring possibilities of signal wakes */
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,
            &pinfo->next_period, NULL);
}


void EtherCATMaster::*simple_cyclic_task(void *data)
{
    struct period_info pinfo;

    periodic_task_init(&pinfo);

    while (1) {
        do_rt_task();
        wait_rest_of_period(&pinfo);
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
    CPU_SET(1, &cpuset); // 绑定到 CPU1
    pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpuset);

    /* Lock memory */
    if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
        printf("mlockall() failed: %m\n");
        exit(-2);
    }

    /* Initialize pthread attributes (default values) */
    ret = pthread_attr_init(&attr);
    if (ret) {
        printf("init pthread attributes failed\n");
        goto out;
    }

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

    param.sched_priority = 80;
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
    ret = pthread_create(&thread, &attr, simple_cyclic_task, NULL);
    if (ret) {
        printf("create pthread failed: %s\n", strerror(ret));
        goto out;
    }

    ret = pthread_setname_np(thread, "cyclic-rt-1-ms");
    if (ret) {
        printf("failed to set thread name\n");
    }

    /* Join the thread and wait until it is done */
    ret = pthread_join(thread, NULL);
    if (ret) {
        printf("join pthread failed: %m\n");
    }

out:
    return ret;
}

void EtherCATMaster::stack_prefault(void)
{
    unsigned char dummy[MY_STACK_SIZE];

    memset(dummy, 0, MY_STACK_SIZE);
}


void EtherCATMaster::init_master(){
    master = ecrt_request_master(0);
    if (!master) {
        return false;
    }

    domain1 = ecrt_master_create_domain(master);
    if (!domain1) {
        return false;
    }

    if (!(sc_1 = ecrt_master_slave_config(
                    master, FirstSlavePos, TI5MOTOR))) {
        fprintf(stderr, "Failed to get slave configuration.\n");
        return false;
    }

    printf("Configuring PDOs...\n");
    if (ecrt_slave_config_pdos(sc_1, EC_END, slave_0_syncs)) {
        fprintf(stderr, "Failed to configure PDOs.\n");
        return false;
    }

  

    if (ecrt_domain_reg_pdo_entry_list(domain1, domain1_regs)) {
        fprintf(stderr, "PDO entry registration failed!\n");
        return false;
    }
    printf("off_control_word=%u off_status_word=%u off_mode=%u off_pos=%u off_mode_display=%u\n",
       off_control_word, off_status_word, off_mode, off_pos, off_mode_display);

    ecrt_slave_config_dc(sc_1, 0x0300, PERIOD_NS, 0, 0, 0);
    printf("Activating master...\n");
    if (ecrt_master_activate(master)) {
        return false;
    }

    if (!(domain1_pd = ecrt_domain_data(domain1))) {
        return false;
    }
    return true;
}