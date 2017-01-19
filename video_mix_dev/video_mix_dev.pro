TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

DEFINES = __STDC_CONSTANT_MACROS
INCLUDEPATH=/home/huheng/source/ffmpeg
LIBS=-lavcodec -lavfilter -lavutil -lswresample \
-lavformat -lswscale  -lavdevice -lpostproc -lpthread

SOURCES += main.cpp \
    filter.cpp \
    demux_decode.cpp \
    encode_mux.cpp \
    broadcastingstation.cpp

HEADERS += \
    safequeue.h \
    filter.h \
    demux_decode.h \
    encode_mux.h \
    util.h \
    broadcastingstation.h
