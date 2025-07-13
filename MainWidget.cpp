#include "MainWidget.h"
#include "ui_MainWidget.h"
#include <QDebug>
#include <QIcon>

MainWidget::MainWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MainWidget)
{
    ui->setupUi(this);

    connect(ui->pushButton, &QPushButton::clicked, this, &MainWidget::slot_RecordButtonClicked);
}

MainWidget::~MainWidget()
{
    delete ui;
}

void MainWidget::slot_RecordButtonClicked(bool checked)
{
    qDebug() << "record:" << checked;
    if (checked) {
        ui->pushButton->setIcon(QIcon(":/images/Resource/images/RecordButton_Stop_bg.png"));
        ui->openGLWidget->startRecord();
    }
    else {
        ui->pushButton->setIcon(QIcon(":/images/Resource/images/RecordButton_Start_bg.png"));
        ui->openGLWidget->stopRecord();
    }
}