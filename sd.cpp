#define AnaInSlavePos 8192, 0
#define ZeroErr 0x5a65726f, 0x00029252
#define frequency 2000
#define MY_STACK_SIZE 8192
 
#include <time.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <ecrt.h>
 
static ec_master_t *master = NULL;
static ec_domain_t *domain = NULL;
static ec_domain_state_t domain_state = {};
static uint8_t *domain_pd = NULL;
 
 
struct ZeroErr_PDO
{
    unsigned int Target_Position;
    unsigned int Digital_outputs;
    unsigned int Control_word;
    unsigned int Position_Actual_Value;
    unsigned int Digital_inputs;
    unsigned int Status_Word;
};
struct ZeroErr_PDO pdos;
 
 
/* Master 0, Slave 0, "ZeroErr Driver"
 * Vendor ID:       0x5a65726f
 * Product code:    0x00029252
 * Revision number: 0x00000001
 */
ec_pdo_entry_info_t slave_0_pdo_entries[] = {
    {0x607a, 0x00, 32}, /* Target Position */
    {0x60fe, 0x00, 32}, /* Digital outputs */
    {0x6040, 0x00, 16}, /* Control Word */
    {0x6064, 0x00, 32}, /* Position Actual Value */
    {0x60fd, 0x00, 32}, /* Digital inputs */
    {0x6041, 0x00, 16}, /* Status Word */
};
 
ec_pdo_info_t slave_0_pdos[] = {
    {0x1600, 3, slave_0_pdo_entries + 0}, /* R0PDO */
    {0x1a00, 3, slave_0_pdo_entries + 3}, /* T0PDO */
};
 
ec_sync_info_t slave_0_syncs[] = {
    {0, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE},
    {1, EC_DIR_INPUT, 0, NULL, EC_WD_DISABLE},
    {2, EC_DIR_OUTPUT, 1, slave_0_pdos + 0, EC_WD_ENABLE},
    {3, EC_DIR_INPUT, 1, slave_0_pdos + 1, EC_WD_DISABLE},
    {0xff}
};
 
const static ec_pdo_entry_reg_t domain_regs[] = {
    //  {Alias, Position, Vendor ID, Product Code, PDO Index, -Subindex, Offset}
    {AnaInSlavePos, ZeroErr, 0x6041, 0x00,&pdos.Status_Word},
    {AnaInSlavePos, ZeroErr, 0x6040, 0x00, &pdos.Control_word},
    {AnaInSlavePos, ZeroErr, 0x607a, 0x00, &pdos.Target_Position},
    {AnaInSlavePos, ZeroErr, 0x6064, 0x00, &pdos.Position_Actual_Value}
};
 
 
void check_domain_state(void){
    ec_domain_state_t ds;
    ecrt_domain_state(domain , &ds);
    if(ds.working_counter != domain_state.working_counter)
    {
        printf("发生了不完整的数据帧传输，当前工作计数器为%u\n",ds.working_counter);
    }
    if(ds.wc_state != domain_state.wc_state){
        printf("工作计数器状态改变为%u\n",ds.wc_state);
        domain_state = ds;
    }
}
//发送消息的频率设置
// 计时的时候需要使用到
struct period_info
{
    struct timespec next_period;
    long period_ns;
};
 
/****************************************************************************/
 
static void inc_period(struct period_info *pinfo)
{
    pinfo->next_period.tv_nsec += pinfo->period_ns;
 
    while (pinfo->next_period.tv_nsec >= 1000000000)
    {
        /* timespec nsec overflow */
        pinfo->next_period.tv_sec++;
        pinfo->next_period.tv_nsec -= 1000000000;
    }
}
 
/****************************************************************************/
 
static void periodic_task_init(struct period_info *pinfo)
{
    /* for simplicity, hardcoding a 100 ms period */
    pinfo->period_ns = 1000000000/frequency;
 
    /* find timestamp to first run */
    clock_gettime(CLOCK_MONOTONIC, &(pinfo->next_period));
}
 
 
static void wait_rest_of_period(struct period_info *pinfo)
{
    inc_period(pinfo);
 
    /* for simplicity, ignoring possibilities of signal wakes */
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,
                    &pinfo->next_period, NULL);
}
 
// 定义循环任务
uint16_t ctrl_word[] = {0x06, 0x07, 0xF};
uint16_t clearword = 0x80;
 
static void do_rt_task(){
    ecrt_master_receive(master);
    ecrt_domain_process(domain);
    check_domain_state();
    int16_t raw_value = EC_READ_S16(domain_pd+ pdos.Status_Word);
 
    static int  i = 0;
    i++;
    ec_domain_state_t ds;
    if(!(i%1000))printf("Status word:%x",raw_value);
    ecrt_domain_state(domain, &ds);
    if(raw_value == 0x1208){
        EC_WRITE_U16(domain_pd + pdos.Control_word, clearword);
    }
    if(raw_value == 0x1250){
        EC_WRITE_U16(domain_pd + pdos.Control_word, ctrl_word[0]);
    }
    if(raw_value == 0x1231){
        EC_WRITE_U16(domain_pd + pdos.Control_word, ctrl_word[1]);
    }
    if(raw_value == 0x1233){
        EC_WRITE_U16(domain_pd + pdos.Control_word, ctrl_word[2]);
    }
    if(raw_value == 0x1237||raw_value == 0x1637){
        EC_WRITE_U16(domain_pd + pdos.Target_Position, i);
    }
     
    ecrt_domain_queue(domain);
 
    ecrt_master_send(master);
}
 
void *simple_cyclic_task (void *data){
    printf("循环任务开始哦\n");
    struct period_info pinfo;
    periodic_task_init(&pinfo);
    while(1){
        do_rt_task();
        wait_rest_of_period(&pinfo);
    }
}
 
int init_ethercat(){
       //启动主站
    ec_slave_config_t *sc;
 
    master = ecrt_request_master(0);
    if(!master){
        return -1;
    }
    domain = ecrt_master_create_domain(master);
    if(!domain){
        return -1;
    }
    if(!(sc = ecrt_master_slave_config(master,AnaInSlavePos,ZeroErr))){
        fprintf(stderr, "配置从站失败\n");
        return -1;
    }
    printf("配置PDO信息\n");
    if(ecrt_slave_config_pdos(sc,EC_END,slave_0_syncs)){
        fprintf(stderr, "对齐PDO信息失败\n");
        return -1;
    }
    //在domain中注册PDO条目
    if(ecrt_domain_reg_pdo_entry_list(domain,domain_regs)){
        fprintf(stderr, "pdo入口注册失败\n");
        return -1;
    }
    //激活master
    if(ecrt_master_activate(master)){
        return -1;
    }
    printf("激活master成功\n");
    if(!(domain_pd = ecrt_domain_data(domain))){
        return -1;
    }
     
    return 0;
}
 
int main(){
    //启动主站，启动线程
    struct sched_param param;
        pthread_attr_t attr;
        pthread_t thread;
        int ret;
 
        ret = init_ethercat();
        printf("主站启动成功");
        if (ret)
        {
            printf("Failed to init EtherCAT master.\n");
            exit(-2);
        }
 
        /* Lock memory */
        if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1)
        {
            printf("mlockall() failed: %m\n");
            exit(-2);
        }
 
        /* Initialize pthread attributes (default values) */
        ret = pthread_attr_init(&attr);
        if (ret)
        {
            printf("init pthread attributes failed\n");
            goto out;
        }
 
        /* Set a specific stack size  */
        ret = pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN + MY_STACK_SIZE);
        if (ret)
        {
            printf("pthread setstacksize failed\n");
            goto out;
        }
 
        /* Set scheduler policy and priority of pthread */
        ret = pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
        if (ret)
        {
            printf("pthread setschedpolicy failed\n");
            goto out;
        }
 
        param.sched_priority = 80;
        ret = pthread_attr_setschedparam(&attr, ¶m);
        if (ret)
        {
            printf("pthread setschedparam failed\n");
            goto out;
        }
 
        /* Use scheduling parameters of attr */
        ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
        if (ret)
        {
            printf("pthread setinheritsched failed\n");
            goto out;
        }
 
        /* Create a pthread with specified attributes */
        ret = pthread_create(&thread, &attr, simple_cyclic_task, NULL);
        if (ret)
        {
            printf("create pthread failed: %s\n", strerror(ret));
            goto out;
        }
 
       ret = pthread_setname_np(thread, "ethercat-rt");
        if (ret)
        {
            printf("failed to set thread name\n");
        }
 
        /* Join the thread and wait until it is done */
        ret = pthread_join(thread, NULL);
        if (ret)
        {
            printf("join pthread failed: %m\n");
        }
 
    out:
        return ret;
 
}