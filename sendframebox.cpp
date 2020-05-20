/****************************************************************************
**
** Copyright (C) 2017 Andre Hartmann <aha_1980@gmx.de>
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

#include "sendframebox.h"
#include "ui_sendframebox.h"

enum {
    MaxStandardId = 0x7FF,
    MaxExtendedId = 0x10000000
};

enum {
    MaxPayload = 8,
    MaxPayloadFd = 64
};

HexIntegerValidator::HexIntegerValidator(QObject *parent) :
    QValidator(parent),
    m_maximum(MaxStandardId)
{
}

QValidator::State HexIntegerValidator::validate(QString &input, int &) const
{
    bool ok;
    uint value = input.toUInt(&ok, 16);

    if (input.isEmpty())
        return Intermediate;

    if (!ok || value > m_maximum)
        return Invalid;

    return Acceptable;
}

void HexIntegerValidator::setMaximum(uint maximum)
{
    m_maximum = maximum;
}

HexStringValidator::HexStringValidator(QObject *parent) :
    QValidator(parent),
    m_maxLength(MaxPayload)
{
}

QValidator::State HexStringValidator::validate(QString &input, int &pos) const
{
    const int maxSize = 2 * m_maxLength;
    const QChar space = QLatin1Char(' ');
    QString data = input;
    data.remove(space);

    if (data.isEmpty())
        return Intermediate;

    // limit maximum size and forbid trailing spaces
    if ((data.size() > maxSize) || (data.size() == maxSize && input.endsWith(space)))
        return Invalid;

    // check if all input is valid
    const QRegularExpression re(QStringLiteral("^[[:xdigit:]]*$"));
    if (!re.match(data).hasMatch())
        return Invalid;

    // insert a space after every two hex nibbles
    int numSpace = ((data.size() - 1) >> 1);
    while (numSpace) {
        data.insert(numSpace * 2, space);
        numSpace--;
    }

    input = data;
    pos = input.size();

    return Acceptable;
}

void HexStringValidator::setMaxLength(int maxLength)
{
    m_maxLength = maxLength;
}

SendFrameBox::SendFrameBox(QWidget *parent) :
    QTabWidget(parent),
    m_ui(new Ui::SendFrameBox)
{
    m_ui->setupUi(this);

    m_hexIntegerValidator = new HexIntegerValidator(this);
    m_ui->frameIdEdit->setValidator(m_hexIntegerValidator);
    m_hexStringValidator = new HexStringValidator(this);
    m_ui->payloadEdit->setValidator(m_hexStringValidator);

    connect(m_ui->extendedFormatBox, &QCheckBox::toggled, [this](bool set) {
        m_hexIntegerValidator->setMaximum(set ? MaxExtendedId : MaxStandardId);
    });

    m_hexStringValidator->setMaxLength(MaxPayload);

    // can
    auto frameCanChanged = [this]() {
        const bool hasFrameId = !m_ui->frameIdEdit->text().isEmpty();
        m_ui->sendButton->setEnabled(hasFrameId);
        m_ui->sendButton->setToolTip(hasFrameId
                                     ? QString() : tr("Cannot send because no Frame ID was given."));
    };
    connect(m_ui->frameIdEdit, &QLineEdit::textChanged, frameCanChanged);
    frameCanChanged();

    connect(m_ui->sendButton, &QPushButton::clicked, [this]() {
        const uint frameId = m_ui->frameIdEdit->text().toUInt(nullptr, 16);
        QString data = m_ui->payloadEdit->text();
        const QByteArray payload = QByteArray::fromHex(data.remove(QLatin1Char(' ')).toLatin1());

        CANMSG_U frame;
        frame.can_frame.IdType = m_ui->extendedFormatBox->isChecked() ? CAN_EXT_ID : CAN_STD_ID;
        frame.can_frame.FrameType = m_ui->remoteFrame->isChecked() ? CAN_REMOTE_FRAME : CAN_DATA_FRAME;
        frame.can_frame.Id = frameId;
        frame.can_frame.DLC = static_cast<unsigned char>(payload.size());
        for (unsigned char i = 0; i < 8; i++) {
            if (i < frame.can_frame.DLC) {
                frame.can_frame.Data[i] = payload[i];
            } else {
                frame.can_frame.Data[i] = 0;
            }
        }

        QByteArray QData = QByteArray::fromRawData(frame.can_raw, sizeof(CANMSG_U));

        emit sendFrame(QData);
    });

    // i2c - read
    auto frameI2cReadChanged = [this]() {
        const bool i2cReadValid = (!m_ui->i2cReadAddrEdit->text().isEmpty())
                && (!m_ui->i2cReadSizeEdit->text().isEmpty());
        m_ui->i2cReadButton->setEnabled(i2cReadValid);
        m_ui->i2cReadButton->setToolTip(i2cReadValid
                                     ? QString() : tr("Both Addr. and Length can not be empty."));
    };
    connect(m_ui->i2cReadAddrEdit, &QLineEdit::textChanged, frameI2cReadChanged);
    connect(m_ui->i2cReadSizeEdit, &QLineEdit::textChanged, frameI2cReadChanged);
    frameI2cReadChanged();

    // i2c - write
    auto frameI2cWriteChanged = [this]() {
        const bool i2cWriteValid = (!m_ui->i2cWriteAddrEdit->text().isEmpty())
                && (!m_ui->i2cWritePlainTextEdit->document()->isEmpty());
        m_ui->i2cWriteButton->setEnabled(i2cWriteValid);
        m_ui->i2cWriteButton->setToolTip(i2cWriteValid
                                     ? QString() : tr("Both Addr. and Data can not be empty."));
    };
    connect(m_ui->i2cWriteAddrEdit, &QLineEdit::textChanged, frameI2cWriteChanged);
    connect(m_ui->i2cWritePlainTextEdit, &QPlainTextEdit::textChanged, frameI2cWriteChanged);
    frameI2cWriteChanged();

    // i2c - read data
    connect(m_ui->i2cReadButton, &QPushButton::clicked, [this]() {
        const uint i2cAddr = m_ui->i2cReadAddrEdit->text().toUInt(nullptr, 16);
        const uint i2cSize = m_ui->i2cReadSizeEdit->text().toUInt(nullptr, 16);
        uint i2cData = (i2cAddr & 0xFFFF) | ((i2cSize & 0xFFFF) << 16) | 0x80;
        QByteArray QData = QByteArray::fromRawData((const char *)&i2cData, sizeof(uint));
        emit sendFrame(QData);
    });

    // i2c - write data
    connect(m_ui->i2cWriteButton, &QPushButton::clicked, [this]() {
        const uint i2cAddr = m_ui->i2cWriteAddrEdit->text().toUInt(nullptr, 16);
        QString data = m_ui->i2cWritePlainTextEdit->toPlainText();
        QByteArray QData = QByteArray::fromHex(data.simplified().remove(QLatin1Char(' ')).toLatin1());
        QData.push_front((char)0);
        QData.push_front((char)i2cAddr);
        emit sendFrame(QData);
    });

    // spi
    auto frameSpiChanged = [this]() {
        const bool spiValid = !m_ui->spiPlainTextEdit->document()->isEmpty();
        m_ui->spiSendButton->setEnabled(spiValid);
        m_ui->spiSendButton->setToolTip(spiValid
                                     ? QString() : tr("Data can not be empty."));
    };
    connect(m_ui->spiPlainTextEdit, &QPlainTextEdit::textChanged, frameSpiChanged);
    frameSpiChanged();

    // spi - read / write data
    connect(m_ui->spiSendButton, &QPushButton::clicked, [this]() {
        QString data = m_ui->spiPlainTextEdit->toPlainText();
        const QByteArray QData = QByteArray::fromHex(data.simplified().remove(QLatin1Char(' ')).toLatin1());
        emit sendFrame(QData);
    });

    // Validator - i2c address
    m_i2cAddrValidator = new HexIntegerValidator(this);
    m_ui->i2cReadAddrEdit->setValidator(m_i2cAddrValidator);
    m_ui->i2cWriteAddrEdit->setValidator(m_i2cAddrValidator);
    m_i2cAddrValidator->setMaximum(0x7F);

    // Validator - i2c read size
    m_i2cReadValidator = new HexIntegerValidator(this);
    m_ui->i2cReadSizeEdit->setValidator(m_i2cReadValidator);
    m_i2cReadValidator->setMaximum(0x200);
}

SendFrameBox::~SendFrameBox()
{
    delete m_ui;
}
