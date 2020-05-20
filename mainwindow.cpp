/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the QtSerialBus module.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "settingsdialog.h"
#include "console.h"
#include "Logger.h"

#include <QCloseEvent>
#include <QDesktopServices>
#include <QTimer>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    m_ui(new Ui::MainWindow),
    m_status(new QLabel),
    m_written(new QLabel),
    m_settings(new SettingsDialog),
    m_serial(new QSerialPort(this)),
    m_console(new Console)
{
    m_ui->setupUi(this);
    m_ui->verticalLayout_4->addWidget(m_console);
    m_ui->receivedMessagesEdit->hide();
    m_ui->label_3->hide(); // If I remove this label from ui, compiler can't find class "QLabel"

    m_ui->statusBar->addPermanentWidget(m_status);

    m_ui->statusBar->addWidget(m_written);

    initActionsConnections();

#if (QT_VERSION >= QT_VERSION_CHECK(5, 8, 0))
    // https://doc.qt.io/qt-5/qserialport.html
    // This function was introduced in Qt 5.8.
    connect(m_serial, &QSerialPort::errorOccurred, this, &MainWindow::processErrors);
#else
    connect(m_serial, SIGNAL(error(QSerialPort::SerialPortError)), this,
            SLOT(processErrors(QSerialPort::SerialPortError)));
#endif
    connect(m_serial, &QSerialPort::readyRead, this, &MainWindow::processReceivedFrames);

    for (int i = 0; i < 3; i++) {
        m_arrWidgets[i] = m_ui->sendFrameBox->widget(i);
    }
    m_ui->sendFrameBox->setTabBarAutoHide(true);
}

MainWindow::~MainWindow()
{
    delete m_settings;
    delete m_ui;
}

void MainWindow::initActionsConnections()
{
    m_ui->actionDisconnect->setEnabled(false);

    connect(m_ui->sendFrameBox, &SendFrameBox::sendFrame, this, &MainWindow::sendFrame);
    connect(m_ui->actionConnect, &QAction::triggered, m_settings, &SettingsDialog::show);
    connect(m_settings, &QDialog::accepted, this, &MainWindow::openSerialPort);
    connect(m_ui->actionDisconnect, &QAction::triggered, this, &MainWindow::closeSerialPort);
    connect(m_ui->actionQuit, &QAction::triggered, this, &QWidget::close);
    connect(m_ui->actionAboutQt, &QAction::triggered, qApp, &QApplication::aboutQt);
    connect(m_ui->actionClearLog, &QAction::triggered, m_ui->receivedMessagesEdit, &QTextEdit::clear);
    connect(m_ui->actionClearLog, &QAction::triggered, m_console, &Console::clear);
}

void MainWindow::processErrors(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::ResourceError) {
        QMessageBox::critical(this, tr("Critical Error"), m_serial->errorString());
        closeSerialPort();
    }
}

void MainWindow::openSerialPort()
{
    const SettingsDialog::Settings p = m_settings->settings();
    m_serial->setPortName(p.name);
    m_serial->setBaudRate(p.baudRate);
    m_serial->setDataBits(p.dataBits);
    m_serial->setParity(p.parity);
    m_serial->setStopBits(p.stopBits);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);
    if (m_serial->open(QIODevice::ReadWrite)) {
        m_serial->setDataTerminalReady(true);
        m_deviceConnected = true;
        m_numberFramesWritten = 0;
        m_ui->actionConnect->setEnabled(false);
        m_ui->actionDisconnect->setEnabled(true);
        if (p.normalModeEnabled) {
            m_ui->sendFrameBox->show();
        } else {
            m_ui->sendFrameBox->hide();
        }
        m_status->setText(tr("Connected to %1").arg(p.name));

        m_ui->sendFrameBox->clear();
        m_ui->sendFrameBox->insertTab(0, m_arrWidgets[p.brgMode], tr(""));

        m_mode = p.brgMode;
        if (m_mode == BRG_MODE_CAN) { // for CAN interface only
            // QByteArray ba = "CANC";
            QByteArray ba;
            int i = 0;
            ba.resize(32);
            ba[i++] = 'C';
            ba[i++] = 'A';
            ba[i++] = 'N';
            ba[i++] = 'C';

            ba[i++] = 1;
            ba[i++] = 0;
            ba[i++] = 0;
            ba[i++] = 0;

            ba[i++] = (p.baudRate & 0xFF);
            ba[i++] = (p.baudRate >> 8) & 0xFF;
            ba[i++] = (p.baudRate >> 16) & 0xFF;
            ba[i++] = (p.baudRate >> 24) & 0xFF;

            ba[i++] = (p.normalModeEnabled ? 0 : 1);
            ba[i++] = 0;
            ba[i++] = 0;
            ba[i++] = 0;

            ba[i++] = (p.canID[0] & 0xFF);
            ba[i++] = (p.canID[0] >> 8) & 0xFF;
            ba[i++] = (p.canID[0] >> 16) & 0xFF;
            ba[i++] = (p.canID[0] >> 24) & 0xFF;

            ba[i++] = (p.canID[1] & 0xFF);
            ba[i++] = (p.canID[1] >> 8) & 0xFF;
            ba[i++] = (p.canID[1] >> 16) & 0xFF;
            ba[i++] = (p.canID[1] >> 24) & 0xFF;

            ba[i++] = (p.canID[2] & 0xFF);
            ba[i++] = (p.canID[2] >> 8) & 0xFF;
            ba[i++] = (p.canID[2] >> 16) & 0xFF;
            ba[i++] = (p.canID[2] >> 24) & 0xFF;

            ba[i++] = (p.canID[3] & 0xFF);
            ba[i++] = (p.canID[3] >> 8) & 0xFF;
            ba[i++] = (p.canID[3] >> 16) & 0xFF;
            ba[i++] = (p.canID[3] >> 24) & 0xFF;
            m_serial->write(ba);
            // m_serial->flush();
        }

        if (m_logger == 0) {
            m_logger = new Logger(this, "NuBridgeLog.txt");
        }

        m_logger->write("Open " + p.name);
    } else {
        m_deviceConnected = false;
        QMessageBox::critical(this, tr("Error"), m_serial->errorString());

        m_status->setText(tr("Open error"));
        m_ui->sendFrameBox->clear();
        m_ui->sendFrameBox->insertTab(0, m_arrWidgets[0], tr("CAN"));
        m_ui->sendFrameBox->insertTab(1, m_arrWidgets[1], tr("I2C"));
        m_ui->sendFrameBox->insertTab(2, m_arrWidgets[2], tr("SPI"));

        if (m_logger != 0) {
            delete m_logger;
            m_logger = nullptr;
        }
    }
}

void MainWindow::closeSerialPort()
{
    if (m_serial->isOpen())
        m_serial->close();

    if (m_logger != 0) {
        delete m_logger;
        m_logger = nullptr;
    }

    m_deviceConnected = false;
    m_ui->actionConnect->setEnabled(true);
    m_ui->actionDisconnect->setEnabled(false);

    m_ui->sendFrameBox->show();

    m_status->setText(tr("Disconnected"));
    m_ui->sendFrameBox->clear();
    m_ui->sendFrameBox->insertTab(0, m_arrWidgets[0], tr("CAN"));
    m_ui->sendFrameBox->insertTab(1, m_arrWidgets[1], tr("I2C"));
    m_ui->sendFrameBox->insertTab(2, m_arrWidgets[2], tr("SPI"));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_settings->close();
    event->accept();
}

void MainWindow::processReceivedFrames()
{
    const QByteArray data = m_serial->readAll();
    m_console->putData(data.toHex(' '));
    m_console->putData("\r\n");

    if (m_logger != 0) {
        m_logger->write(data.toHex(' '));
    }
}

void MainWindow::sendFrame(const QByteArray &frame) const
{
    if (m_deviceConnected) { // On-Line mode
        if (m_mode == BRG_MODE_CAN) { // for can only
            QByteArray data("CAND");
            data.append(frame);
            m_serial->write(data);
        } else { // for i2c & spi
            // for debug purpose only, printf output data
            // m_console->putData("\r\nOffLine: ");
            // m_console->putData(frame.toHex());
            m_serial->setRequestToSend(true);
            m_serial->write(frame);
            m_serial->setRequestToSend(false);
        }
    } else { // Off-Line Mode
        m_console->putData("\nOffline: ");
#if (QT_VERSION >= QT_VERSION_CHECK(5, 9, 0))
        // https://doc.qt.io/qt-5/qbytearray.html#toHex
        // This function was introduced in Qt 5.9.
        m_console->putData(frame.toHex(' '));
#else
        m_console->putData(frame.toHex());
#endif
    }
}
