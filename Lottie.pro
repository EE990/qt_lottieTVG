QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# 啟用 pkg-config 支援
CONFIG += link_pkgconfig

# 引入 thorvg (pkg-config 會自動處理 include 路徑與 -lthorvg 連結)
PKGCONFIG += thorvg

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    thorvgwidget.cpp

HEADERS += \
    mainwindow.h \
    thorvgwidget.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
