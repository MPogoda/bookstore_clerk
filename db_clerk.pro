#-------------------------------------------------
#
# Project created by QtCreator 2013-02-20T03:00:46
#
#-------------------------------------------------

QT       += core gui sql

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = db_clerk
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    logindialog.cpp \
    fillrequestdialog.cpp

HEADERS  += mainwindow.h \
    logindialog.h \
    fillrequestdialog.h

FORMS    += mainwindow.ui \
    logindialog.ui \
    fillrequestdialog.ui

OTHER_FILES +=
