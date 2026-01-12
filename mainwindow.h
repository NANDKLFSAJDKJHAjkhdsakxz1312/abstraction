#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "zdltask.h"

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
    void onSliderValueChanged_0(int32_t value);
    // void onSliderValueChanged_1(int32_t value);
    // void onSliderValueChanged_2(int32_t value);
    void init_qt();
    void sendcw_qt();
    void changemode_qt();
    void start_rt_thread_qt();

private:
    Ui::MainWindow *ui;
    ZDLTask *igh_master;
};
#endif // MAINWINDOW_H
