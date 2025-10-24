#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QDebug>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    
    igh_master = new EtherCATMaster();
    
    connect(ui->slider_0, &QSlider::valueChanged, this, &MainWindow::onSliderValueChanged_0);
    // connect(ui->slider_1, &QSlider::valueChanged, this, &MainWindow::onSliderValueChanged_1);
    // connect(ui->slider_2, &QSlider::valueChanged, this, &MainWindow::onSliderValueChanged_2);
    connect(ui->initButton, &QPushButton::clicked, this, &MainWindow::init_qt);
    connect(ui->sendcwButton, &QPushButton::clicked, this, &MainWindow::sendcw_qt);
    connect(ui->changemodeButton, &QPushButton::clicked, this, &MainWindow::changemode_qt);
    connect(ui->startrtthreadButton, &QPushButton::clicked, this, &MainWindow::start_rt_thread_qt);
}

MainWindow::~MainWindow()
{
    delete ui;
    delete igh_master;
}

void MainWindow::onSliderValueChanged_0(int32_t value)
{
    
    ui->valuelabel_0->setText(QString("当前值: %1").arg(value));
    igh_master->send_target_pos_0(value);
}

// void MainWindow::onSliderValueChanged_1(int32_t value)
// {
    
//     ui->valuelabel_1->setText(QString("当前值: %1").arg(value));
//     igh_master->send_target_pos_1(value);
// }

// void MainWindow::onSliderValueChanged_2(int32_t value)
// {
    
//     ui->valuelabel_2->setText(QString("当前值: %1").arg(value));
//     igh_master->send_target_pos_2(value);
// }



void MainWindow::init_qt(){
    // 初始化主站
    if(!igh_master->init_master()){
        printf("初始化失败！\n");
    }
    else{
        printf("初始化成功！\n");
    }
    // 配置实时线程参数并创建线程
    
}


void MainWindow::sendcw_qt()
{
    QString text = ui->cwlineEdit->text().trimmed();
    bool ok = false;
    uint16_t cw_word = 0;

    if (text.startsWith("0x", Qt::CaseInsensitive)) {
        cw_word = text.toUShort(&ok, 16);
    } else {
        cw_word = text.toUShort(&ok, 10);
    }// 静态成员初始化




    if (!ok) {
        qWarning() << "Invalid control word input:" << text;
        return;
    }

    if (!igh_master) {
        qWarning() << "EtherCATMaster instance is null!";
        return;
    }

    // 根据输入设置对应 flag
    if (cw_word == 0x06) {
        igh_master->send_control_word06(cw_word);
    } else if (cw_word == 0x07) {
        igh_master->send_control_word07(cw_word);
    } else if (cw_word == 0x0F) {
        igh_master->send_control_word0f(cw_word);
    } else if (cw_word == 0x80) {
        igh_master->send_control_word80(cw_word);
    } else {
        qWarning() << "Undefined control word:" << QString("0x%1").arg(cw_word, 4, 16, QChar('0'));
        return;
    }

    qDebug() << "Set control word flag for:" << QString("0x%1").arg(cw_word, 4, 16, QChar('0'));
}


void MainWindow::changemode_qt()
{
    // 从界面获取模式输入
    QString text = ui->modelineEdit->text().trimmed();
    bool ok = false;
    int8_t mode = 0;

    // 判断是否十六进制输入
    if (text.startsWith("0x", Qt::CaseInsensitive)) {
        mode = static_cast<int8_t>(text.toInt(&ok, 16));
    } else {
        mode = static_cast<int8_t>(text.toInt(&ok, 10));
    }

    if (!ok) {
        qWarning() << "Invalid mode input:" << text;
        return;
    }

    if (!igh_master) {
        qWarning() << "EtherCATMaster instance is null!";
        return;
    }

    // 调用主站接口发送模式字
    igh_master->send_mode_of_operation(mode);

    qDebug() << "Sent mode of operation:" << QString("0x%1").arg(mode, 2, 16, QChar('0'));
}


void MainWindow::start_rt_thread_qt(){
    igh_master->config_rt_params_and_create_pthread();
}