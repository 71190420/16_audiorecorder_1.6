QT       += core gui serialport
QT       += multimedia printsupport

QT += widgets virtualkeyboard


greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
greaterThan(QT_MAJOR_VERSION,4):QT += widgets printsupport

CONFIG += c++11

DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
    barchartmainwindow.cpp \
    gps.cpp \
    main.cpp \
    audiorecorder.cpp \
    newwindow.cpp \
    qcustomplot.cpp

HEADERS += \
    audiorecorder.h \
    barchartmainwindow.h \
    gps.h \
    newwindow.h \
    qcustomplot.h

RESOURCES += \
    res.qrc

FORMS += \
    barchartmainwindow.ui \
    gps.ui \
    newwindow.ui

# 部署路径
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
