#include "zdltask.h"
#include <iostream>
#include <unistd.h>
#include <signal.h>

// -----------------------------------------------------------
// 1. 定义全局应用层指针（对应 PDO 条目）
// -----------------------------------------------------------
uint16_t *ctrl_word = nullptr;   // 0x6040:00
uint16_t *status_word = nullptr; // 0x6041:00
int32_t  *target_pos = nullptr;  // 0x607A:00

bool run_loop = true;

// 信号处理：优雅退出
void signal_handler(int sig) {
    run_loop = false;
}

// -----------------------------------------------------------
// 2. 配置回调 (注册 PDO)
// -----------------------------------------------------------
void my_config_callback(ZDLTask &task) {
    printf("--- 开始配置从站 PDO ---\n");

    // 定义要注册的从站位置和索引
    uint16_t slave_pos = 0; // 假设第一个从站
    pdo_entry_idx idx_ctrl = {0x6040, 0x00};
    pdo_entry_idx idx_status = {0x6041, 0x00};

    // 尝试注册。注意：这里利用了你编写的模板函数
    // addr 会在 task.start() 内部通过 perform_delayed_binding 自动赋值
    if (!task.try_register_pdo_entry(ctrl_word, slave_pos, idx_ctrl, nullptr, nullptr)) {
        printf("注册控制字失败！\n");
    }
    if (!task.try_register_pdo_entry(status_word, slave_pos, idx_status, nullptr, nullptr)) {
        printf("注册状态字失败！\n");
    }
    
    printf("--- PDO 注册指令已提交 ---\n");
}

// -----------------------------------------------------------
// 3. 周期性控制回调 (在实时线程运行)
// -----------------------------------------------------------
void my_pdo_callback() {
    static int cycle_count = 0;

    // 检查指针是否已成功绑定（由 perform_delayed_binding 完成）
    if (ctrl_word && status_word) {
        // 读取状态字
        uint16_t current_status = *status_word;

        // 简单的状态机逻辑：每 1000 个周期切换一次控制字
        if (cycle_count % 1000 == 0) {
            *ctrl_word = 0x000F; // 示例：使能电机
            printf("实时线程: Status=0x%04X, 写入 Control=0x000F\n", current_status);
        }
    }
    cycle_count++;
}

int main() {
    signal(SIGINT, signal_handler);

    ZDLTask task;

    // A. 设置任务属性 (CPU 核心 1, 优先级 80, 周期 1ms (1000000ns), Master ID 0)
    if (task.set_task_attribute(1, 80, 1000000, 0) != 0) {
        std::cerr << "设置任务属性失败" << std::endl;
        return -1;
    }

    // B. 设置配置回调：在 Master 激活前调用
    task.set_config_callback([&]() {
        my_config_callback(task);
    });

    // C. 设置实时循环回调：在 simple_cyclic_task 中每周期执行
    task.set_cycle_callback(my_pdo_callback);

    // D. 启动任务 (内部会执行：config_callback -> activate -> perform_delayed_binding -> start thread)
    printf("正在启动 EtherCAT 任务...\n");
    if (task.start() != 0) {
        std::cerr << "任务启动失败" << std::endl;
        return -1;
    }

    printf("任务已启动，输入 Ctrl+C 停止。\n");

    // 主线程等待，实时工作在后台线程完成
    while (run_loop) {
        sleep(1);
    }

    printf("正在停止并退出...\n");
    return 0;
}