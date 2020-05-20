/****************************************************************************
**
** Copyright (C) 2012 Denis Shienkov <denis.shienkov@gmail.com>
** Copyright (C) 2012 Laszlo Papp <lpapp@kde.org>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtSerialPort module of the Qt Toolkit.
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

#include "settingsdialog.h"
#include "ui_settingsdialog.h"

#include <QIntValidator>
#include <QLineEdit>
#include <QSerialPortInfo>

#include "sendframebox.h"

static const char blankString[] = QT_TRANSLATE_NOOP("SettingsDialog", "N/A");

SettingsDialog::SettingsDialog(QWidget *parent) :
    QDialog(parent),
    m_ui(new Ui::SettingsDialog),
    m_intValidator(new QIntValidator(0, 4000000, this))
{
    m_ui->setupUi(this);

    m_ui->baudRateBox->setInsertPolicy(QComboBox::NoInsert);

#if (QT_VERSION >= QT_VERSION_CHECK(5, 7, 0))
    connect(m_ui->serialPortInfoListBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::showPortInfo);
#else
    connect(m_ui->serialPortInfoListBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(showPortInfo(int)));
#endif

    // Scan Port
    connect(m_ui->serialPortInfoListBox,SIGNAL(activated(int)),this,SLOT(clickPortInfo(int)));

    connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::ok);
    connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &SettingsDialog::cancel);


    fillCanParameters();
    fillI2cParameters();
    fillSpiParameters();
    fillPortsInfo();


    m_hexIntegerValidatorStd = new HexIntegerValidator(this);
    m_ui->msg0StdIdEdit->setValidator(m_hexIntegerValidatorStd);
    m_ui->msg1StdIdEdit->setValidator(m_hexIntegerValidatorStd);
    m_hexIntegerValidatorExt = new HexIntegerValidator(this);
    m_ui->msg2ExtIdEdit->setValidator(m_hexIntegerValidatorExt);
    m_ui->msg3ExtIdEdit->setValidator(m_hexIntegerValidatorExt);

    m_hexIntegerValidatorStd->setMaximum(0x7FF);
    m_hexIntegerValidatorExt->setMaximum(0x10000000);

    /* Don't know how to use auto on QComboBox */
    // auto canModeChanged = [this](int) {
    //     const bool normalModeEnabled = m_ui->canModeBox->itemData(m_ui->canModeBox->currentIndex()).toInt();
    //     m_ui->msg0StdIdEdit->setEnabled(normalModeEnabled);
    //     m_ui->msg1StdIdEdit->setEnabled(normalModeEnabled);
    //     m_ui->msg2ExtIdEdit->setEnabled(normalModeEnabled);
    //     m_ui->msg3ExtIdEdit->setEnabled(normalModeEnabled);
    // };
#if (QT_VERSION >= QT_VERSION_CHECK(5, 7, 0))
    connect(m_ui->canModeBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::canModeChanged);
#else
    connect(m_ui->canModeBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(canModeChanged(int)));
#endif

    m_mode = BRG_MODE_I2C; // default is I2C Mode
    m_ui->brgModeBox->addItem(tr("CAN"), 0);
    m_ui->brgModeBox->addItem(tr("I2C"), 1);
    m_ui->brgModeBox->addItem(tr("SPI"), 2);
    m_ui->brgModeBox->setCurrentIndex(m_mode);
    m_ui->parametersBox->setCurrentIndex(m_mode);

    connect(m_ui->brgModeBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::brgModeChanged);
    connect(m_ui->parametersBox, SIGNAL(currentChanged(int)),
            this, SLOT(brgModeChanged(int)));

    brgModeChanged(m_mode);


    updateSettings();
}

SettingsDialog::~SettingsDialog()
{
    delete m_ui;
}

SettingsDialog::Settings SettingsDialog::settings() const
{
    return m_currentSettings;
}

void SettingsDialog::clickPortInfo(int idx)
{
    if (idx == 0) {
        fillPortsInfo();
        return;
    }
}

void SettingsDialog::showPortInfo(int idx)
{
    if (idx == -1)
        return;

    const QStringList list = m_ui->serialPortInfoListBox->itemData(idx).toStringList();
    m_ui->descriptionLabel->setText(tr("Description: %1").arg(list.count() > 1 ? list.at(1) : tr(blankString)));
    m_ui->manufacturerLabel->setText(tr("Manufacturer: %1").arg(list.count() > 2 ? list.at(2) : tr(blankString)));
    m_ui->serialNumberLabel->setText(tr("Serial number: %1").arg(list.count() > 3 ? list.at(3) : tr(blankString)));
    m_ui->locationLabel->setText(tr("Location: %1").arg(list.count() > 4 ? list.at(4) : tr(blankString)));
    m_ui->vidLabel->setText(tr("Vendor Identifier: %1").arg(list.count() > 5 ? list.at(5) : tr(blankString)));
    m_ui->pidLabel->setText(tr("Product Identifier: %1").arg(list.count() > 6 ? list.at(6) : tr(blankString)));
}

void SettingsDialog::ok()
{
    updateSettings();
    accept();
}

void SettingsDialog::cancel()
{
    revertSettings();
    reject();
}

void SettingsDialog::brgModeChanged(int idx)
{
    m_mode = idx;

    if (m_ui->parametersBox->currentIndex() != idx) {
        m_ui->parametersBox->setCurrentIndex(idx);
    }

    if (m_ui->brgModeBox->currentIndex() != idx) {
        m_ui->brgModeBox->setCurrentIndex(idx);
    }
}

void SettingsDialog::canModeChanged(int idx)
{
    Q_UNUSED(idx)
    const bool normalModeEnabled = m_ui->canModeBox->itemData(m_ui->canModeBox->currentIndex()).toInt();
    m_ui->msg0StdIdEdit->setEnabled(normalModeEnabled);
    m_ui->msg1StdIdEdit->setEnabled(normalModeEnabled);
    m_ui->msg2ExtIdEdit->setEnabled(normalModeEnabled);
    m_ui->msg3ExtIdEdit->setEnabled(normalModeEnabled);
}

void SettingsDialog::fillCanParameters()
{
    const QList<int> rates = {
        1000000, 500000, 250000, 125000, 50000, 10000
    };

    for (int rate : rates)
        m_ui->baudRateBox->addItem(QString::number(rate), rate);

    m_ui->baudRateBox->setCurrentIndex(1); // default is 500000 bits/sec

    m_ui->canModeBox->addItem(tr("Silent"), 0);
    m_ui->canModeBox->addItem(tr("Normal"), 1);
    m_ui->canModeBox->setCurrentIndex(1); // default is Normal Mode
}

void SettingsDialog::fillI2cParameters()
{
    // 6.27 USCI - I2C Mode: Communication in standard mode (100 kBit/s) or in fast mode (up to 400 kBit/s)
    const QList<int> rates = {
        400000, 100000
    };

    for (int rate : rates)
        m_ui->i2cClockBox->addItem(QString::number(rate), rate);

    m_ui->i2cClockBox->setCurrentIndex(1); // default is 100 kBit/s

    m_ui->i2cModeBox->addItem(tr("Monitor"), 0);
    m_ui->i2cModeBox->addItem(tr("Master"), 1);
    m_ui->i2cModeBox->setCurrentIndex(1); // default is Master Mode
}

void SettingsDialog::fillSpiParameters()
{
    const QList<int> rates = {
        1000000, 2000000, 3000000, 4000000, 6000000, 8000000, 12000000, 16000000, 24000000
    };

    for (int rate : rates)
        m_ui->spiClockBox->addItem(QString::number(rate), rate);

    m_ui->spiClockBox->setCurrentIndex(0); // default is 1Mhz

    m_ui->spiTypeBox->addItem(tr("Type0"), 0);
    m_ui->spiTypeBox->addItem(tr("Type1"), 1);
    m_ui->spiTypeBox->addItem(tr("Type2"), 2);
    m_ui->spiTypeBox->addItem(tr("Type3"), 3);
    m_ui->spiTypeBox->setCurrentIndex(0); // default is Type0

    m_ui->spiModeBox->addItem(tr("Monitor"), 0);
    m_ui->spiModeBox->addItem(tr("Master"), 1);
    // m_ui->spiModeBox->addItem(tr("Slave"), 2);
    m_ui->spiModeBox->setCurrentIndex(1); // default is Master Mode

    m_ui->spiOrderBox->addItem(tr("MSB First"), 0);
    m_ui->spiOrderBox->addItem(tr("LSB First"), 1);
    m_ui->spiOrderBox->setCurrentIndex(0); // default is MSB First

    m_ui->spiSsActiveBox->addItem(tr("Low"), 0);
    m_ui->spiSsActiveBox->addItem(tr("High"), 1);
    m_ui->spiSsActiveBox->setCurrentIndex(0); // default is SS Active Low
}

void SettingsDialog::fillPortsInfo()
{
    m_ui->serialPortInfoListBox->clear();
    m_ui->serialPortInfoListBox->addItem(tr("Scan Port"));
    QString description;
    QString manufacturer;
    QString serialNumber;
    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) {
        QStringList list;
        description = info.description();
        manufacturer = info.manufacturer();
        serialNumber = info.serialNumber();
        list << info.portName()
             << (!description.isEmpty() ? description : blankString)
             << (!manufacturer.isEmpty() ? manufacturer : blankString)
             << (!serialNumber.isEmpty() ? serialNumber : blankString)
             << info.systemLocation()
             << (info.vendorIdentifier() ? QString::number(info.vendorIdentifier(), 16) : blankString)
             << (info.productIdentifier() ? QString::number(info.productIdentifier(), 16) : blankString);

        m_ui->serialPortInfoListBox->addItem(list.first(), list);
    }
}

void SettingsDialog::updateSettings()
{
    m_currentSettings.brgMode = m_mode;

    // COM port
    m_currentSettings.name = m_ui->serialPortInfoListBox->currentText();
    if (m_mode == BRG_MODE_I2C) {
        m_currentSettings.baudRate = m_ui->i2cClockBox->itemData(m_ui->i2cClockBox->currentIndex()).toInt();
        m_currentSettings.normalModeEnabled = m_ui->i2cModeBox->itemData(m_ui->i2cModeBox->currentIndex()).toInt();
        m_currentSettings.stopBits = !m_currentSettings.normalModeEnabled ? QSerialPort::OneStop : QSerialPort::TwoStop;
        m_currentSettings.parity = QSerialPort::NoParity;
        m_currentSettings.dataBits = QSerialPort::Data8;
        return;

    } else if (m_mode == BRG_MODE_SPI) {
        m_currentSettings.baudRate = m_ui->spiClockBox->itemData(m_ui->spiClockBox->currentIndex()).toInt();
        m_currentSettings.normalModeEnabled = m_ui->spiModeBox->itemData(m_ui->spiModeBox->currentIndex()).toInt();

        QSerialPort::StopBits spiMode[3] = {QSerialPort::OneStop, QSerialPort::OneAndHalfStop, QSerialPort::TwoStop};
        m_currentSettings.stopBits = spiMode[m_ui->spiModeBox->itemData(m_ui->spiModeBox->currentIndex()).toInt()];

        QSerialPort::Parity spiType[4] = {QSerialPort::NoParity, QSerialPort::OddParity, QSerialPort::EvenParity, QSerialPort::MarkParity};
        m_currentSettings.parity = spiType[m_ui->spiTypeBox->itemData(m_ui->spiTypeBox->currentIndex()).toInt()];

        QSerialPort::DataBits spiMisc[4] = {QSerialPort::Data8, QSerialPort::Data5, QSerialPort::Data6, QSerialPort::Data7};
        m_currentSettings.dataBits = spiMisc[m_ui->spiOrderBox->currentIndex() + 2 * m_ui->spiSsActiveBox->currentIndex()];
        return;
    }

    m_currentSettings.baudRate = m_ui->baudRateBox->itemData(m_ui->baudRateBox->currentIndex()).toInt();
    m_currentSettings.logFileEnabled = m_ui->logFileCheckBox->isChecked();
    m_currentSettings.normalModeEnabled = m_ui->canModeBox->itemData(m_ui->canModeBox->currentIndex()).toInt();

    m_currentSettings.canID[0] = 0xFFFFFFFF;
    m_currentSettings.canID[1] = 0xFFFFFFFF;
    m_currentSettings.canID[2] = 0xFFFFFFFF;
    m_currentSettings.canID[3] = 0xFFFFFFFF;

    if (!m_ui->msg0StdIdEdit->text().isEmpty()) {
        m_currentSettings.canID[0] = m_ui->msg0StdIdEdit->text().toUInt(nullptr, 16);
    }

    if (!m_ui->msg1StdIdEdit->text().isEmpty()) {
        m_currentSettings.canID[1] = m_ui->msg1StdIdEdit->text().toUInt(nullptr, 16);
    }

    if (!m_ui->msg2ExtIdEdit->text().isEmpty()) {
        m_currentSettings.canID[2] = m_ui->msg2ExtIdEdit->text().toUInt(nullptr, 16);
    }

    if (!m_ui->msg3ExtIdEdit->text().isEmpty()) {
        m_currentSettings.canID[3] = m_ui->msg3ExtIdEdit->text().toUInt(nullptr, 16);
    }
}

void SettingsDialog::revertSettings()
{
}
