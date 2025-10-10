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