#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "master.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onSliderValueChanged(int value);
    void init_qt();

private:
    Ui::MainWindow *ui;
    EtherCATMaster *igh_master;
};
#endif // MAINWINDOW_H
