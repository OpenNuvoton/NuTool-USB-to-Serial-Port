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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QMutex>
#include <QThread>
#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include "nuvbridge.h"

QT_BEGIN_NAMESPACE

class QLabel;
class SettingsDialog;
class Console;
class Logger;

namespace Ui
{
class MainWindow;
}

QT_END_NAMESPACE

template <class T>
class Buffer : public QObject
{
public:
    explicit Buffer(quint32 size, quint8 value, QObject *parent = nullptr);
    ~Buffer();

public:
    void push_back(T *data, quint32 count);
    void delete_front(quint32 count);
    bool peek_front(quint32 index, T *data);
    quint32 get_count();
    void reset();

private:
    T *m_buffer = nullptr;
    quint32 m_size = 0;
    quint32 m_count = 0;
    quint32 m_head = 0;
    quint32 m_tail = 0;
    QMutex m_countMutex;
    QMutex m_headMutex;
    QMutex m_tailMutex;
};


class Formatter : public QObject
{
    Q_OBJECT
public:
    typedef struct {
        char buffer[40 << 10];
        quint32 size;
    } FORMAT_BLOCK_T;

    typedef struct {
        quint32 index;
        quint64 timestamp;
        FORMAT_BLOCK_T block;
    } RECORD_T;

public:
    explicit Formatter(QObject *parent = nullptr);
    ~Formatter();

    void regFrame(void *frames, Buffer<FORMAT_BLOCK_T> *blocks, void (*formatter)(RECORD_T *record, void *frames, Buffer<FORMAT_BLOCK_T> *blocks));
    void reset();

signals:
    void signal_deletePortObject();

private slots:
    void slot_init();
    void slot_update();
    void slot_flush();

private:
    QTimer *m_timer = nullptr;
    RECORD_T m_record;
    void *m_frames = nullptr;
    void (*m_formatter)(RECORD_T *record, void *frames, Buffer<FORMAT_BLOCK_T> *blocks) = nullptr;
    Buffer<FORMAT_BLOCK_T> *m_blocks = nullptr;
    QMutex m_mutex;
};


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void aboutNuTool();

signals:
    void signal_formatterFlush();

private slots:
    void processReceivedFrames();
    void sendFrame(const QByteArray &frame) const;
    void openSerialPort();
    void closeSerialPort();
    void processErrors(QSerialPort::SerialPortError error);
    void readFrames();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void initActionsConnections();

    qint64 m_numberFramesWritten = 0;
    Ui::MainWindow *m_ui = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_written = nullptr;
    SettingsDialog *m_settings = nullptr;
    QSerialPort *m_serial = nullptr;
    Console *m_console = nullptr;
    bool m_deviceConnected = false;
    QWidget *m_arrWidgets[4];
    qint32 m_mode = 0;
    Buffer<Formatter::FORMAT_BLOCK_T> *m_blocks = nullptr;
    Formatter *m_formatter = nullptr;
    QThread *m_thread = nullptr;
    QTimer *m_timer = nullptr;
    void *m_frames = nullptr;
    Logger *m_logger = nullptr;
};

#endif // MAINWINDOW_H
