QT       += core gui multimedia sql widgets charts
CONFIG += c++17

# OpenCV configuration for your specific installation
INCLUDEPATH += C:/Users/bouib/Desktop/opencv/build/include

# For release configuration
CONFIG(release, debug|release): {
    LIBS += -LC:/Users/bouib/Desktop/opencv/build/x64/vc16/lib \
            -lopencv_world4110
}

# For debug configuration
CONFIG(debug, debug|release): {
    LIBS += -LC:/Users/bouib/Desktop/opencv/build/x64/vc16/lib \
            -lopencv_world4110d
}
  QT += webenginewidgets
  QT += pdf pdfwidgets
  QT += network
  QT += axcontainer
SOURCES += \
    main.cpp \
    mainwindow.cpp \
    pomodoro.cpp \
    studygoals.cpp \
    achievements.cpp \
    streak.cpp \
    analytics.cpp \
    achievementitemdelegate.cpp \
     studysession.cpp \
     survey.cpp

HEADERS += \
    mainwindow.h \
    pomodoro.h \
    studygoals.h \
    achievements.h \
    streak.h \
    analytics.h \
    achievementitemdelegate.h \
      studysession.h \
      survey.h 

FORMS += \
    mainwindow.ui

# Resource files
RESOURCES += \
    resources.qrc

# Copy required files to build directory
QMAKE_POST_LINK += \
    $(COPY) $$PWD\\resources\\haarcascade_frontalface_alt.xml $$OUT_PWD && \
    $(COPY) $$PWD\\resources\\haarcascade_eye.xml $$OUT_PWD

# Default rules for deployment
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
