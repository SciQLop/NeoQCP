#pragma once

#include <QMainWindow>
#include <QVBoxLayout>

#include <qcustomplot.h>


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();


private:
    QVBoxLayout *mLayout;
};

