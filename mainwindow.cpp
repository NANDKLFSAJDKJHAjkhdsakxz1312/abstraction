#include "mainwindow.h"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    connect(ui->slider, &QSlider::valueChanged, this, &MainWindow::onSliderValueChanged);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onSliderValueChanged(int value)
{
  
    ui->label->setText(QString("当前值: %1").arg(value));
}
