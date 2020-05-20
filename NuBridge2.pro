QT += widgets serialport

# CONFIG += C++11
CONFIG += c++14

TARGET = NuBridge2_terminal
TEMPLATE = app

SOURCES += \
    settingsdialog.cpp \
    main.cpp \
    mainwindow.cpp \
    sendframebox.cpp \
    console.cpp \
    Logger.cpp

HEADERS += \
    settingsdialog.h \
    mainwindow.h \
    sendframebox.h \
    console.h \
    Logger.h

FORMS   += mainwindow.ui \
    settingsdialog.ui \
    sendframebox.ui

HEADERS += nuvbridge.h

RESOURCES += can.qrc

