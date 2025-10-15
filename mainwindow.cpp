#include "mainwindow.h"
#include "./ui_mainwindow.h"



MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    igh_master = new EtherCATMaster();
    connect(ui->slider, &QSlider::valueChanged, this, &MainWindow::onSliderValueChanged);
    connect(ui->initButton, &QPushButton::clicked, this, &MainWindow::init_qt);
}

MainWindow::~MainWindow()
{
    delete ui;
    delete igh_master;
}

void MainWindow::onSliderValueChanged(int value)
{
    igh_master->config_rt_params_and_create_pthread();
    ui->valuelabel->setText(QString("当前值: %1").arg(value));
}


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