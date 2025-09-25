/*****************************************************************************
 *
 *  Copyright (C) 2007-2009  Florian Pose, Ingenieurgemeinschaft IgH
 *
 *  This file is part of the IgH EtherCAT Master.
 *
 *  The IgH EtherCAT Master is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License version 2, as
 *  published by the Free Software Foundation.
 *
 *  The IgH EtherCAT Master is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 *  Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with the IgH EtherCAT Master; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 ****************************************************************************/

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
#include <stdbool.h>

/****************************************************************************/

#include "ecrt.h"

#define STAT_READY_TO_SWITCH_ON (1<<0)
#define STAT_SWITCHED_ON        (1<<1)
#define STAT_OPERATION_ENABLED  (1<<2)
#define STAT_FAULT              (1<<3)
#define STAT_SWITCH_ON_DISABLED (1<<6)
/****************************************************************************/

/** Task period in ns. */
#define PERIOD_NS   (1000000)

#define MAX_SAFE_STACK (8 * 1024) /* The maximum stack size which is
                                     guranteed safe to access without
                                     faulting */

/****************************************************************************/

/* Constants */
#define NSEC_PER_SEC (1000000000)
#define FREQUENCY (NSEC_PER_SEC / PERIOD_NS)

/****************************************************************************/

// EtherCAT
static ec_master_t *master = NULL;
static ec_master_state_t master_state = {};

static ec_domain_t *domain1 = NULL;
static ec_domain_state_t domain1_state = {};

static ec_slave_config_t *sc_ana_in = NULL;
static ec_slave_config_state_t sc_ana_in_state = {};

/****************************************************************************/

// process data
static uint8_t *domain1_pd = NULL;


#define FirstSlavePos  0, 0
#define SecondSlavePos  0, 1


#define TI5MOTOR 0x00522227, 0x00009253




// offsets for PDO entries
static unsigned int off_control_word;
static unsigned int off_mode_of_operation;
static unsigned int off_target_pos;
// static unsigned int off_target_vel;
// static unsigned int off_target_tor;
static unsigned int off_status_word;
static unsigned int off_actual_pos;
// static unsigned int off_actual_vel;
// static unsigned int off_actual_tor;
static unsigned int off_error_code;
static unsigned int off_mode_of_operation_display;

// offsets for PDO entries
static unsigned int off_control_word_2;
static unsigned int off_mode_of_operation_2;
static unsigned int off_target_pos_2;
// static unsigned int off_target_vel_2;
// static unsigned int off_target_tor_2;
static unsigned int off_status_word_2;
static unsigned int off_actual_pos_2;
// static unsigned int off_actual_vel_2;
// static unsigned int off_actual_tor_2;
static unsigned int off_error_code_2;
static unsigned int off_mode_of_operation_display_2;




const static ec_pdo_entry_reg_t domain1_regs[] = {
    {FirstSlavePos,  TI5MOTOR, 0x607a, 0, &off_target_pos},
    {FirstSlavePos,  TI5MOTOR, 0x6060, 0, &off_mode_of_operation},
    {FirstSlavePos,  TI5MOTOR, 0x6040, 0, &off_control_word},
    
    
    
    // {FirstSlavePos,  TI5MOTOR, 0x60ff, 0, &off_target_vel},
    // {FirstSlavePos,  TI5MOTOR, 0x6071, 0, &off_target_tor},
    {FirstSlavePos,  TI5MOTOR, 0x6064, 0, &off_actual_pos},
    {FirstSlavePos,  TI5MOTOR, 0x6041, 0, &off_status_word},
    {FirstSlavePos,  TI5MOTOR, 0x6061, 0, &off_mode_of_operation_display},
    
    
    // {FirstSlavePos,  TI5MOTOR, 0x606c, 0, &off_actual_vel},
    // {FirstSlavePos,  TI5MOTOR, 0x6077, 0, &off_actual_tor},
    
    {FirstSlavePos,  TI5MOTOR, 0x603f, 0, &off_error_code},
    // {SecondSlavePos,  TI5MOTOR, 0x6060, 0, &off_mode_of_operation_2},
    // {SecondSlavePos,  TI5MOTOR, 0x6040, 0, &off_control_word_2},
    // {SecondSlavePos,  TI5MOTOR, 0x607a, 0, &off_target_pos_2},
    // // {SecondSlavePos,  TI5MOTOR, 0x60ff, 0, &off_target_vel_2},
    // // {SecondSlavePos,  TI5MOTOR, 0x6071, 0, &off_target_tor_2},
    // {SecondSlavePos,  TI5MOTOR, 0x6064, 0, &off_actual_pos_2},
    // {SecondSlavePos,  TI5MOTOR, 0x6041, 0, &off_status_word_2},
    // {SecondSlavePos,  TI5MOTOR, 0x606c, 0, &off_actual_vel_2},
    // {SecondSlavePos,  TI5MOTOR, 0x6077, 0, &off_actual_tor_2},
    // {SecondSlavePos,  TI5MOTOR, 0x6061, 0, &off_mode_of_operation_display_2},
    // {SecondSlavePos,  TI5MOTOR, 0x603f, 0, &off_error_code_2},
  
    {}
};

static unsigned int counter = 0;
static unsigned int blink = 0;

/****************************************************************************/

// out --------------------------

static const ec_pdo_entry_info_t out_pdo_entries[] = {
    {0x607A, 0, 32} ,
    {0x6060, 0, 8},   /* Mode of Operation (RxPDO) */
    {0x6040, 0, 16} /* Controlword */
      /* Target position */
};
static const ec_pdo_info_t out_pdos[] = {
    {0x1600, 3, out_pdo_entries}
};

/* INPUT (从站 -> 主站) */
static const ec_pdo_entry_info_t in_pdo_entries[] = {
    {0x6064, 0, 32},
    {0x6041, 0, 16},  /* Statusword */
    {0x6061, 0, 8},   /* Mode of operation display (TxPDO) */
      /* Actual position */
    {0x603f, 0, 16}
};
static const ec_pdo_info_t in_pdos[] = {
    {0x1A00, 4, in_pdo_entries}
};

/* Syncs：注意 sync index 与设备 EDS 要匹配（大多数驱动器使用 2=Rx, 3=Tx） */
static const ec_sync_info_t motor_syncs[] = {
    {2, EC_DIR_OUTPUT, 1, out_pdos}, /* RxPDOs */
    {3, EC_DIR_INPUT,  1, in_pdos},  /* TxPDOs */
    {0xff}
};



/****************************************************************************/

void check_domain1_state(void)
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

/****************************************************************************/

void check_master_state(void)
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

/****************************************************************************/

void check_slave_config_states(void)
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

/****************************************************************************/
static int step = 0;
static int toggle = 1;
static bool exeFlag = true;
static uint16_t status;
static uint16_t control_word;
static int32_t actual_pos;
static int8_t mode_disp;
static int8_t mode;
static uint16_t errorcode;
static int32_t target_pos;
uint16_t cw = 0;
static int i = 0;
void cyclic_task()
{
    
    ++i;
    // receive process data
    ecrt_master_receive(master);
    ecrt_domain_process(domain1);

    // check process data state
    check_domain1_state();
    status = EC_READ_U16(domain1_pd + off_status_word);
    actual_pos   = EC_READ_S32(domain1_pd+ off_actual_pos);

    mode_disp     = EC_READ_S8(domain1_pd + off_mode_of_operation_display); 
    mode    = EC_READ_S8(domain1_pd + off_mode_of_operation);
    errorcode = EC_READ_U16(domain1_pd + off_error_code);
    target_pos = EC_READ_S32(domain1_pd+ off_target_pos);
    control_word = EC_READ_U16(domain1_pd + off_control_word);
    if(i%1000==0){
        printf("actual_pos: %d，control word: %d,target_pos: %d,status word:%d,error code: %d,mode: %d,mode_dis: %d\n",
        actual_pos,control_word,target_pos,status,errorcode,mode,mode_disp);
    }
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
    }

#if 0
    // read process data
    printf("AnaIn: state %u value %u\n",
            EC_READ_U8(domain1_pd + off_ana_in_status),
            EC_READ_U16(domain1_pd + off_ana_in_value));
#endif

#if 1
        if (status & STAT_FAULT) {
            // 如果有 Fault，先尝试复位（多数驱动用 0x80 或 0x06 做 Reset/Shutdown）
            cw = 0x80; // 或厂商手册规定的 FaultReset 值
        } else if (status & STAT_SWITCH_ON_DISABLED) {
            // 如果处于 Switch On Disabled，发送 Shutdown -> Ready to switch on
            cw = 0x06;
        } else if ((status & STAT_READY_TO_SWITCH_ON) && !(status & STAT_SWITCHED_ON)) {
            // Ready to Switch On -> 发送 Switch On
            cw = 0x07;
        } else if ((status & STAT_SWITCHED_ON) && !(status & STAT_OPERATION_ENABLED)) {
            // Switched On -> 发送 Enable Operation
            cw = 0x0F;
            EC_WRITE_S8(domain1_pd + off_mode_of_operation, 0X08);
        } else {
            // 已经 Operation Enabled，保持你想要的 controlword（如 0x0F）
            cw = 0x0F;
        }

        // 只在需要时写入，避免无谓的总线负载
        static uint16_t last_cw = 0;
        if (cw != last_cw) {
            EC_WRITE_U16(domain1_pd + off_control_word, cw);
            last_cw = cw;
        }

        EC_WRITE_S32(domain1_pd + off_target_pos, i*1);
        


#endif

    // send process data
    ecrt_domain_queue(domain1);
    ecrt_master_send(master);
}

/****************************************************************************/

void stack_prefault(void)
{
    unsigned char dummy[MAX_SAFE_STACK];

    memset(dummy, 0, MAX_SAFE_STACK);
}

/****************************************************************************/

int main(int argc, char **argv)
{
    ec_slave_config_t *sc;
    struct timespec wakeup_time;
    int ret = 0;

    master = ecrt_request_master(0);
    if (!master) {
        return -1;
    }

    domain1 = ecrt_master_create_domain(master);
    if (!domain1) {
        return -1;
    }
    // first slave config
    if (!(sc_ana_in = ecrt_master_slave_config(
                    master, FirstSlavePos, TI5MOTOR))) {
        fprintf(stderr, "Failed to get slave configuration.\n");
        return -1;
    }

    // printf("Configuring PDOs...\n");
    // if (ecrt_slave_config_pdos(sc_ana_in, EC_END, in_syncs)) {
    //     fprintf(stderr, "Failed to configure PDOs.\n");
    //     return -1;
    // }

    if (ecrt_slave_config_pdos(sc_ana_in, EC_END, motor_syncs)) {
        fprintf(stderr, "Failed to configure PDOs.\n");
        return -1;
    }



    // second slave config
    // if (!(sc = ecrt_master_slave_config(
    //                 master, SecondSlavePos, TI5MOTOR))) {
    //     fprintf(stderr, "Failed to get slave configuration.\n");
    //     return -1;
    // }

    // if (ecrt_slave_config_pdos(sc, EC_END, in_syncs)) {
    //     fprintf(stderr, "Failed to configure PDOs.\n");
    //     return -1;
    // }

    // if (ecrt_slave_config_pdos(sc, EC_END, out_syncs)) {
    //     fprintf(stderr, "Failed to configure PDOs.\n");
    //     return -1;
    // }
    

    if (ecrt_domain_reg_pdo_entry_list(domain1, domain1_regs)) {
        fprintf(stderr, "PDO entry registration failed!\n");
        return -1;
    }
    printf("off_mode=%u off_mode_disp=%u off_status=%u  off_error_code=%u\n",
       off_mode_of_operation, off_mode_of_operation_display, off_status_word,off_error_code);

    printf("Activating master...\n");
    if (ecrt_master_activate(master)) {
        return -1;
    }

    if (!(domain1_pd = ecrt_domain_data(domain1))) {
        return -1;
    }

    /* Set priority */

    struct sched_param param = {};
    param.sched_priority = sched_get_priority_max(SCHED_FIFO);

    printf("Using priority %i.\n", param.sched_priority);
    if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
        perror("sched_setscheduler failed");
    }

    /* Lock memory */

    if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
        fprintf(stderr, "Warning: Failed to lock memory: %s\n",
                strerror(errno));
    }

    stack_prefault();

    printf("Starting RT task with dt=%u ns.\n", PERIOD_NS);

    clock_gettime(CLOCK_MONOTONIC, &wakeup_time);
    wakeup_time.tv_sec += 1; /* start in future */
    wakeup_time.tv_nsec = 0;

    while (1) {
        ret = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,
                &wakeup_time, NULL);
        if (ret) {
            fprintf(stderr, "clock_nanosleep(): %s\n", strerror(ret));
            break;
        }

        cyclic_task();

        wakeup_time.tv_nsec += PERIOD_NS;
        while (wakeup_time.tv_nsec >= NSEC_PER_SEC) {
            wakeup_time.tv_nsec -= NSEC_PER_SEC;
            wakeup_time.tv_sec++;
        }
    }

    return ret;
}

/****************************************************************************/
