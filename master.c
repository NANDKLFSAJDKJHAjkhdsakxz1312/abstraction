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

/****************************************************************************/

#include "ecrt.h"

/****************************************************************************/

/** Task period in ns. */
#define PERIOD_NS   (1000000)

#define MAX_SAFE_STACK (8 * 1024) /* The maximum stack size which is
                                     guranteed safe to access without
                                     faulting */
#include <time.h>
#include <stdint.h>

#ifndef CLOCK_TO_USE
#define CLOCK_TO_USE CLOCK_MONOTONIC
#endif

#ifndef TIMESPEC2NS
#define TIMESPEC2NS(ts) ( (uint64_t)(ts).tv_sec * 1000000000ULL + (uint64_t)(ts).tv_nsec )
#endif

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

#define TI5MOTOR 0x00522227, 0x00009253

// offsets for PDO entries
static unsigned int off_status_word;
static unsigned int off_control_word;
static unsigned int off_mode;
static unsigned int off_pos;
static unsigned int off_mode_display;
static unsigned int off_pos_actual;

const static ec_pdo_entry_reg_t domain1_regs[] = {
    {FirstSlavePos,  TI5MOTOR, 0x6040, 0, &off_control_word},    /* Control Word */
    {FirstSlavePos,  TI5MOTOR, 0x607A, 0, &off_pos},             /* Target Position */
    {FirstSlavePos,  TI5MOTOR, 0x6041, 0, &off_status_word},     /* Status Word */
    {FirstSlavePos,  TI5MOTOR, 0x6060, 0, &off_mode},            /* Mode of Operation (SINT) */
    {FirstSlavePos,  TI5MOTOR, 0x6061, 0, &off_mode_display},    /* Mode of Operation Display (SINT) */
    {FirstSlavePos,  TI5MOTOR, 0x6064, 0, &off_pos_actual},  
    {}
};

static unsigned int counter = 0;
static unsigned int blink = 0;
static unsigned int sync_ref_counter = 0;

static uint16_t status;
static uint16_t ctrl_word = 0;

int8_t mode;
int8_t mode_disp;
int32_t pos_actual;
static int i;
/****************************************************************************/

// Analog in --------------------------

// static const ec_pdo_entry_info_t el3102_pdo_entries[] = {
//     {0x6040, 0 ,16},
//     {0x6401, 0, 16}  // channel 2 value (alt.)
// };

// static const ec_pdo_info_t el3102_pdos[] = {
//     {0x1600, 1, el3102_pdo_entries},
//     {0x1A00, 1, el3102_pdo_entries + 1}
// };

// static const ec_sync_info_t el3102_syncs[] = {
//     {2, EC_DIR_OUTPUT,1 ,el3102_pdos},
//     {3, EC_DIR_INPUT, 1, el3102_pdos+1},
//     {0xff}
// };

// Analog out -------------------------

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

void cyclic_task(struct timespec *time)
{

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
    }

#if 0
    // read process data
    printf("AnaIn: state %u value %u\n",
            EC_READ_U8(domain1_pd + off_ana_in_status),
            EC_READ_U16(domain1_pd + off_ana_in_value));
#endif

#if 1
    // write process data
    status = EC_READ_U16(domain1_pd + off_status_word);
    mode = EC_READ_S8(domain1_pd + off_mode); 
    mode_disp = EC_READ_S8(domain1_pd + off_mode_display);
    pos_actual = EC_READ_S32(domain1_pd + off_pos_actual);
    switch (i) {
    case 5000:
    
        EC_WRITE_U16(domain1_pd + off_control_word, 0x06);
        EC_WRITE_S8(domain1_pd + off_mode, 0x00);
        printf("5000\n");
        break;

    case 10000:
  
        EC_WRITE_U16(domain1_pd + off_control_word, 0x86);
        EC_WRITE_S8(domain1_pd + off_mode, 0x00);
        printf("10000\n");
        break;

    case 15000:
    
        EC_WRITE_U16(domain1_pd + off_control_word, 0x86);
        EC_WRITE_S8(domain1_pd + off_mode, 0x08);
        printf("15000\n");
        break;
    case 20000:
    
        EC_WRITE_U16(domain1_pd + off_control_word, 0x06);
        EC_WRITE_S8(domain1_pd + off_mode, 0x08);
        printf("20000\n");
        break;
    case 25000:
    
        EC_WRITE_U16(domain1_pd + off_control_word, 0x07);
        EC_WRITE_S8(domain1_pd + off_mode, 0x08);
        printf("25000\n");
        break;
    case 30000:
    
        EC_WRITE_U16(domain1_pd + off_control_word, 0x0f);
        EC_WRITE_S8(domain1_pd + off_mode, 0x08);
        printf("30000\n");
        break;
    case 35000:
    
        EC_WRITE_U16(domain1_pd + off_control_word, 0x1f);
        EC_WRITE_S8(domain1_pd + off_mode, 0x08);
        EC_WRITE_S32(domain1_pd + off_pos, 50000);
        i = -1;
        printf("starting moving\n");
        break;
  
        
    default:
        break;
}
  // Switch On Disabled -> Ready to Switch On
    // else if ((status & 0x006F) == 0x0021) ctrl_word = 0x0007; // Ready to Switch On -> Switched On
    // else if ((status & 0x006F) == 0x0023){
    //     EC_WRITE_S8(domain1_pd + off_mode, 0x08);
    //     ctrl_word = 0x000F; // Switched On -> Operation Enabled
    // }
    // if ((status & 0x08) == 0x08) ctrl_word = 0x0080;
    
    // EC_WRITE_U16(domain1_pd + off_control_word, ctrl_word);

    // // 写目标位置只能在 Operation Enabled 时
    // if ((status & 0x006F) == 0x0027) {
    //     i = 50000;
    //     EC_WRITE_S32(domain1_pd + off_pos, i);
    // }
#endif
    if (sync_ref_counter) {
        sync_ref_counter--;
    } else {
        sync_ref_counter = 1; // sync every cycle

        clock_gettime(CLOCK_TO_USE, time);
        ecrt_master_sync_reference_clock_to(master, TIMESPEC2NS(*time));
    }
    ecrt_master_sync_slave_clocks(master);
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
    struct timespec wakeup_time,*time;
    
    int ret = 0;

    master = ecrt_request_master(0);
    if (!master) {
        return -1;
    }

    domain1 = ecrt_master_create_domain(master);
    if (!domain1) {
        return -1;
    }

    if (!(sc_ana_in = ecrt_master_slave_config(
                    master, FirstSlavePos, TI5MOTOR))) {
        fprintf(stderr, "Failed to get slave configuration.\n");
        return -1;
    }

    printf("Configuring PDOs...\n");
    if (ecrt_slave_config_pdos(sc_ana_in, EC_END, slave_0_syncs)) {
        fprintf(stderr, "Failed to configure PDOs.\n");
        return -1;
    }

  

    if (ecrt_domain_reg_pdo_entry_list(domain1, domain1_regs)) {
        fprintf(stderr, "PDO entry registration failed!\n");
        return -1;
    }
    printf("off_control_word=%u off_status_word=%u off_mode=%u off_pos=%u off_mode_display=%u\n",
       off_control_word, off_status_word, off_mode, off_pos, off_mode_display);

    ecrt_slave_config_dc(sc_ana_in, 0x0300, PERIOD_NS, 0, 0, 0);
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

        if(i%1000==0){
            printf("status: 0x%04x,mode: 0x%04x, mode_disp:0x%04x, actual_pos:%d\n", status,mode,mode_disp,pos_actual); 

        }
        ret = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,
                &wakeup_time, NULL);
        if (ret) {
            fprintf(stderr, "clock_nanosleep(): %s\n", strerror(ret));
            break;
        }
        ecrt_master_application_time(master, TIMESPEC2NS(wakeup_time));
        cyclic_task(time);
        if(i!=-1){
            i++;
        }
        
        wakeup_time.tv_nsec += PERIOD_NS;
        while (wakeup_time.tv_nsec >= NSEC_PER_SEC) {
            wakeup_time.tv_nsec -= NSEC_PER_SEC;
            wakeup_time.tv_sec++;
        }
    }

    return ret;
}

/****************************************************************************/
