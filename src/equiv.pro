TEMPLATE	      = app
CONFIG		     += qt warn_on force_debug_info thread c++17
FORMS		      = mainwindow.ui

HEADERS		      = include/colors.h \
                        include/mainwindow.h \
                        include/util-widgets.h

SOURCES		      = main.cc util-widgets.cc \
                        renderer.cc tables.cc

isEmpty(PREFIX) {
PREFIX = /usr/local
}

TARGET                = equiv
DATADIR               = $$PREFIX/share/equiv
DOCDIR                = $$PREFIX/share/doc/equiv

*-g++ {
QMAKE_CXXFLAGS += -fno-diagnostics-show-caret
}

unix:INCLUDEPATH      += include
win32:INCLUDEPATH     += include

!win32:DEFINES       += "DATADIR=\\\"$$DATADIR\\\""
!win32:DEFINES       += "DOCDIR=\\\"$$DOCDIR\\\""
release:DEFINES      += NO_CHECK
win32:DEFINES        += QT_DLL QT_THREAD_SUPPORT HAVE_CONFIG_H _USE_MATH_DEFINES

target.path           = $$PREFIX/bin
INSTALLS += target

QT += widgets gui sql

RESOURCES += \
    equiv.qrc
