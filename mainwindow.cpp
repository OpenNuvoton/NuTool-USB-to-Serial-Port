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

#define TOOL_VERSION    "1.02"

typedef struct {
    quint8 data;
} NORMAL_FRAME_T;

typedef struct {
    quint8 channel;
    quint8 data;
    quint8 reserved[2];
    quint32 timestamp;
} UART_FRAME_T;


template <class T> Buffer<T>::Buffer(quint32 size, quint8 value, QObject *parent)
    : QObject(parent)
    , m_size(size)
{
    m_buffer = new T[m_size];
    memset(m_buffer, value, m_size * sizeof(T));
}

template <class T> Buffer<T>::~Buffer()
{
    delete m_buffer;
    m_buffer = nullptr;
}

template <class T>
void Buffer<T>::push_back(T *data, quint32 count)
{
    const QMutexLocker lock0(&m_headMutex);
    const QMutexLocker lock1(&m_countMutex);

    if ((m_count + count) > m_size)
        return;

    for (quint32 i = 0; i < count; i++) {
        if (data != nullptr)
            m_buffer[m_head] = data[i];

        m_head++;

        if (m_head >= m_size)
            m_head = 0;
    }

    m_count += count;
}

template <class T>
void Buffer<T>::delete_front(quint32 count)
{
    const QMutexLocker lock0(&m_tailMutex);
    const QMutexLocker lock1(&m_countMutex);

    if (m_count < count)
        return;

    m_tail += count;

    if (m_tail >= m_size)
        m_tail -= m_size;

    m_count -= count;
}

template <class T>
bool Buffer<T>::peek_front(quint32 index, T *data)
{
    const QMutexLocker lock0(&m_tailMutex);
    const QMutexLocker lock1(&m_countMutex);
    quint32 tail;

    if (index >= m_count)
        return false;

    tail = m_tail + index;

    if (tail >= m_size)
        tail -= m_size;

    *data = m_buffer[tail];

    return true;
}

template <class T>
quint32 Buffer<T>::get_count()
{
    const QMutexLocker lock(&m_countMutex);
    return m_count;
}

template <class T>
void Buffer<T>::reset()
{
    const QMutexLocker lock0(&m_countMutex);
    const QMutexLocker lock1(&m_headMutex);
    const QMutexLocker lock2(&m_tailMutex);

    m_count = 0;
    m_head = 0;
    m_tail = 0;
}


Formatter::Formatter(QObject *parent)
    : QObject(parent)
{
    m_timer = new QTimer(this);

    memset(&m_record, 0, sizeof(m_record));

    connect(m_timer, SIGNAL(timeout()), this, SLOT(slot_update()));

    m_timer->start(0);
}

Formatter::~Formatter()
{
    emit signal_deletePortObject();

    m_timer->stop();

    delete m_timer;
}

void Formatter::regFrame(void *frames, Buffer<FORMAT_BLOCK_T> *blocks, void (*formatter)(RECORD_T *record, void *frames, Buffer<FORMAT_BLOCK_T> *blocks))
{
    const QMutexLocker lock(&m_mutex);

    m_frames = frames;
    m_blocks = blocks;
    m_formatter = formatter;
}

void Formatter::reset()
{
    const QMutexLocker lock(&m_mutex);

    memset(&m_record, 0, sizeof(m_record));
}

void Formatter::slot_init()
{
}

void Formatter::slot_update()
{
    const QMutexLocker lock(&m_mutex);

    if (m_formatter != nullptr) {
        m_formatter(&m_record, m_frames, m_blocks);
    }
}

void Formatter::slot_flush()
{
    const QMutexLocker lock(&m_mutex);

    if (m_record.block.size > 0) {
        m_blocks->push_back(&m_record.block, 1);
        m_record.block.size = 0;
    }
}


static void formatter_normal(Formatter::RECORD_T *record, void *frames, Buffer<Formatter::FORMAT_BLOCK_T> *blocks)
{
    if (frames != nullptr && blocks != nullptr) {
        quint32 count = static_cast<Buffer<NORMAL_FRAME_T> *>(frames)->get_count();
        NORMAL_FRAME_T frame;

        for (quint32 i = 0; i < count; i++) {
            static_cast<Buffer<NORMAL_FRAME_T> *>(frames)->peek_front(i, &frame);

            sprintf(&(record->block.buffer[record->block.size]), " %02X", frame.data);
            record->block.size += 3;

            if (record->block.size > (sizeof(record->block) - 3)) {
                blocks->push_back(&record->block, 1);
                record->block.size = 0;
            }
        }

        static_cast<Buffer<NORMAL_FRAME_T> *>(frames)->delete_front(count);
    }
}

static void formatter_uart(Formatter::RECORD_T *record, void *frames, Buffer<Formatter::FORMAT_BLOCK_T> *blocks)
{
    if (frames != nullptr && blocks != nullptr) {
        quint32 count = static_cast<Buffer<UART_FRAME_T> *>(frames)->get_count();
        UART_FRAME_T frame;
        quint64 timestamp;

        for (quint32 i = 0; i < count; i++) {
            static_cast<Buffer<UART_FRAME_T> *>(frames)->peek_front(i, &frame);

            timestamp = record->timestamp;
            record->timestamp += frame.timestamp;

            if (frame.channel > 0) {
                if ((timestamp / 1000) != (record->timestamp / 1000)) {
                    std::string timeString = QTime(0, 0, 0, 0).addMSecs(record->timestamp / 1000).toString("hh:mm:ss.zzz").toStdString();

                    if (record->index == 0) {
                        sprintf(&(record->block.buffer[record->block.size]), "[+%s] ", timeString.c_str());
                        record->block.size += 16;
                    } else {
                        sprintf(&(record->block.buffer[record->block.size]), "\n[+%s] ", timeString.c_str());
                        record->block.size += 17;
                    }

                    record->index = 0;

                    if (record->block.size > (sizeof(record->block) - 17)) {
                        blocks->push_back(&record->block, 1);
                        record->block.size = 0;
                    }
                }
            }

            if (frame.channel == 1) {
                sprintf(&(record->block.buffer[record->block.size]), "  %02X", frame.data);
                record->block.size += 4;
                record->index++;
            } else if (frame.channel == 2) {
                sprintf(&(record->block.buffer[record->block.size]), " *%02X", frame.data);
                record->block.size += 4;
                record->index++;
            }

            if (record->block.size > (sizeof(record->block) - 17)) {
                blocks->push_back(&record->block, 1);
                record->block.size = 0;
            }
        }

        static_cast<Buffer<UART_FRAME_T> *>(frames)->delete_front(count);
    }
}


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_ui(new Ui::MainWindow)
    , m_status(new QLabel)
    , m_written(new QLabel)
    , m_settings(new SettingsDialog)
    , m_serial(new QSerialPort(this))
    , m_console(new Console)
    , m_thread(new QThread)
    , m_timer(new QTimer)
{
    m_blocks = new Buffer<Formatter::FORMAT_BLOCK_T>(2 << 10, 0);
    m_formatter = new Formatter();
    m_formatter->moveToThread(m_thread);

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
    connect(m_serial, &QSerialPort::readyRead, this, &MainWindow::readFrames);

    for (qint32 i = 0; i < 4; i++) {
        m_arrWidgets[i] = m_ui->sendFrameBox->widget(i);
    }

    m_ui->sendFrameBox->setTabBarAutoHide(true);

    m_thread->start();
    m_timer->start(0);
}

MainWindow::~MainWindow()
{
    if (m_serial->isOpen())
        m_serial->close();

    m_timer->stop();
    m_thread->quit();
    m_thread->wait();

    if (m_mode == BRG_MODE_UART) {
        if (m_frames != nullptr) {
            delete static_cast<Buffer<UART_FRAME_T> *>(m_frames);
            m_frames = nullptr;
        }
    } else {
        if (m_frames != nullptr) {
            delete static_cast<Buffer<NORMAL_FRAME_T> *>(m_frames);
            m_frames = nullptr;
        }
    }

    if (m_logger != nullptr) {
        delete m_logger;
        m_logger = nullptr;
    }

    delete m_timer;
    delete m_thread;
    delete m_serial;
    delete m_blocks;
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
    connect(m_ui->actionAboutNuTool, &QAction::triggered, this, &MainWindow::aboutNuTool);

    connect(m_formatter, SIGNAL(signal_deletePortObject()), m_thread, SLOT(quit()));
    connect(m_formatter, SIGNAL(signal_deletePortObject()), m_thread, SLOT(deleteLater()));
    connect(m_thread, SIGNAL(started()), m_formatter, SLOT(slot_init()));
    connect(m_thread, SIGNAL(finished()), m_formatter, SLOT(deleteLater()));
    connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);

    connect(this, SIGNAL(signal_formatterFlush()), m_formatter, SLOT(slot_flush()));
    connect(m_timer, SIGNAL(timeout()), this, SLOT(processReceivedFrames()));
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

    bool isNuLink2 = false;

    if (p.usbVendorID == 0x0416) {
        if ((p.usbProductID == 0x5204) || (p.usbProductID == 0x5205) || (p.usbProductID == 0x2008)) {
            isNuLink2 = true;
        }
    }

    // The official NuLink2 fw has merged all the interface into one application.
    // NuLink2 uses the most significant bits in baudRate to switch the interface.
    // NuLink2 adapter vendor id = 0x0416, product id = 0x5204 or 0x5205 or 0x2008

    if (isNuLink2) {
        qint32 baudRate = (p.baudRate & 0x0FFFFFFF) | ((p.brgMode + 1) << 28);
        m_serial->setBaudRate(baudRate);
    } else {
        m_serial->setBaudRate(p.baudRate);
    }

    m_serial->setDataBits(p.dataBits);
    m_serial->setParity(p.parity);
    m_serial->setStopBits(p.stopBits);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    m_formatter->reset();
    m_formatter->regFrame(nullptr, nullptr, nullptr);
    m_blocks->reset();

    if (m_mode == BRG_MODE_UART) {
        if (m_frames != nullptr) {
            delete static_cast<Buffer<UART_FRAME_T> *>(m_frames);
            m_frames = nullptr;
        }
    } else {
        if (m_frames != nullptr) {
            delete static_cast<Buffer<NORMAL_FRAME_T> *>(m_frames);
            m_frames = nullptr;
        }
    }

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

        if (isNuLink2) {
            m_status->setText(tr("Connected to %1 (Nu-Link2)").arg(p.name));
        } else {
            m_status->setText(tr("Connected to %1").arg(p.name));
        }

        m_ui->sendFrameBox->clear();
        m_ui->sendFrameBox->insertTab(0, m_arrWidgets[p.brgMode], tr(""));

        m_mode = p.brgMode;

        if (m_mode == BRG_MODE_CAN) { // for CAN interface only
            // QByteArray ba = "CANC";
            QByteArray ba;
            qint32 i = 0;
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

        if (m_mode == BRG_MODE_UART) {
            m_frames = static_cast<void *>(new Buffer<UART_FRAME_T>((2 << 20) / sizeof(UART_FRAME_T), 0));

            if (static_cast<Buffer<UART_FRAME_T> *>(m_frames) != nullptr) {
                static_cast<Buffer<UART_FRAME_T> *>(m_frames)->reset();
                m_formatter->regFrame(m_frames, m_blocks, formatter_uart);
            }
        } else {
            m_frames = static_cast<void *>(new Buffer<NORMAL_FRAME_T>((2 << 20) / sizeof(NORMAL_FRAME_T), 0));

            if (static_cast<Buffer<NORMAL_FRAME_T> *>(m_frames) != nullptr) {
                static_cast<Buffer<NORMAL_FRAME_T> *>(m_frames)->reset();
                m_formatter->regFrame(m_frames, m_blocks, formatter_normal);
            }
        }

        if (m_logger == nullptr) {
            m_logger = new Logger(this, "LogData.txt");
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
        m_ui->sendFrameBox->insertTab(3, m_arrWidgets[3], tr("UART"));

        if (m_logger != nullptr) {
            delete m_logger;
            m_logger = nullptr;
        }
    }
}

void MainWindow::closeSerialPort()
{
    if (m_serial->isOpen())
        m_serial->close();

    m_formatter->reset();
    m_formatter->regFrame(nullptr, nullptr, nullptr);
    m_blocks->reset();

    if (m_mode == BRG_MODE_UART) {
        if (m_frames != nullptr) {
            delete static_cast<Buffer<UART_FRAME_T> *>(m_frames);
            m_frames = nullptr;
        }
    } else {
        if (m_frames != nullptr) {
            delete static_cast<Buffer<NORMAL_FRAME_T> *>(m_frames);
            m_frames = nullptr;
        }
    }

    if (m_logger != nullptr) {
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
    m_ui->sendFrameBox->insertTab(3, m_arrWidgets[3], tr("UART"));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_settings->close();
    event->accept();
}

void MainWindow::processReceivedFrames()
{
    quint32 count = m_blocks->get_count();

    if (count == 0) {
        emit signal_formatterFlush();
    } else {
        Formatter::FORMAT_BLOCK_T block;

        for (quint32 i = 0; i < count; i++) {
            m_blocks->peek_front(i, &block);

            m_console->putData(static_cast<char *>(block.buffer));

            if (m_logger != nullptr) {
                m_logger->write(static_cast<char *>(block.buffer));
            }

            QCoreApplication::processEvents();
        }

        if (m_mode == BRG_MODE_UART) {
            ;
        } else {
            m_console->putData("\r\n");
        }

        m_blocks->delete_front(count);
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
            m_serial->flush();
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

void MainWindow::readFrames()
{
    if (!m_serial->isOpen()) {
        return;
    }

    QByteArray frames = m_serial->readAll();

    if (frames.isEmpty()) {
        return;
    }

    if (m_mode == BRG_MODE_UART) {
        if ((frames.size() % sizeof(UART_FRAME_T)) == 0) {
            static_cast<Buffer<UART_FRAME_T> *>(m_frames)->push_back(reinterpret_cast<UART_FRAME_T *>(frames.data())
                                                                     , frames.size() / sizeof(UART_FRAME_T));
        }
    } else {
        if ((frames.size() % sizeof(NORMAL_FRAME_T)) == 0) {
            static_cast<Buffer<NORMAL_FRAME_T> *>(m_frames)->push_back(reinterpret_cast<NORMAL_FRAME_T *>(frames.data())
                                                                       , frames.size() / sizeof(NORMAL_FRAME_T));
        }
    }
}

void MainWindow::aboutNuTool()
{
    QString html = "<B>Version " TOOL_VERSION "</B><BR><BR>"
                   "NuTool-USB to Serial Port is based on the following Qt examples.<BR>"
                   "<a href=\"https://doc.qt.io/qt-5/qtserialport-terminal-example.html\">Terminal Example</a>"
                   " and "
                   "<a href=\"https://doc.qt.io/qt-5/qtserialbus-can-example.html\">CAN Bus example</a><BR><BR>"
                   "<B>Github Repository</B><BR>"
                   "<a href=\"https://github.com/OpenNuvoton/NuTool-USB-to-Serial-Port\">OpenNuvoton/NuTool-USB to Serial Port</a><BR>"
                   "<a href=\"https://github.com/OpenNuvoton/Nu-Link2-Bridge_Firmware\">OpenNuvoton/Nu-Link2-Bridge_Firmware</a><BR>";
    QMessageBox *box = new QMessageBox(QMessageBox::NoIcon
                                       , "About NuTool-USB to Serial Port"
                                       , html
                                       , QMessageBox::Ok
                                       , this);
    box->setDefaultButton(QMessageBox::Ok);
    box->exec();
}
