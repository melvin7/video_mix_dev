TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

INCLUDEPATH=/home/huheng/source/ffmpeg
LIBS=-lavcodec -lavfilter -lavutil -lswresample \
-lavformat -lswscale  -lavdevice -lpostproc -lpthread

SOURCES += main.cpp \
    demo.cpp \
    decode.cpp

HEADERS += \
    demo.h \
    safequeue.h
