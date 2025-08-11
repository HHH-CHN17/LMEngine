#include "MainWidget.h"
#include "ui_MainWidget.h"
#include "Common/DataDefine.h"
#include <QDebug>
#include <QIcon>

MainWidget::MainWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MainWidget)
{
    ui->setupUi(this);

    connect(ui->btn_record, &QPushButton::clicked, this, &MainWidget::slot_RecordBtnClicked);
    connect(ui->btn_rtmpPush, &QPushButton::clicked, this, &MainWidget::slot_RtmpPushBtnClicked);
}

MainWidget::~MainWidget()
{
    delete ui;
}

void MainWidget::slot_RecordBtnClicked()
{
	static bool record = false;
	record = !record;
    if (record) {
        ui->btn_record->setIcon(QIcon(":/images/Resource/images/RecordButton_Stop_bg.png"));
        ui->openGLWidget->startRecord(avACT::RECORD);
    }
    else {
        ui->btn_record->setIcon(QIcon(":/images/Resource/images/RecordButton_Start_bg.png"));
        ui->openGLWidget->stopRecord(avACT::RECORD);
    }
}

void MainWidget::slot_RtmpPushBtnClicked()
{
    static bool record = false;
    record = !record;
    if (record) {
        ui->btn_record->setIcon(QIcon(":/images/Resource/images/RecordButton_Stop_bg.png"));
        ui->openGLWidget->startRecord(avACT::RTMPPUSH);
    }
    else {
        ui->btn_record->setIcon(QIcon(":/images/Resource/images/RecordButton_Start_bg.png"));
        ui->openGLWidget->stopRecord(avACT::RTMPPUSH);
    }
}