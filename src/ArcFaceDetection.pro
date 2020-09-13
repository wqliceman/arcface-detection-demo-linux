QT += core gui widgets

CONFIG += c++11

TARGET = demo
TEMPLATE = app

HEADERS += \
	ArcFaceDetection/ArcFaceDetection.h \
	ArcFaceDetection/FaceEngine.h \
	ArcFaceDetection/resource.h \
	ArcFaceDetection/Utils.h \
	ArcFaceDetection/config.h \
	ArcFaceDetection/IrPreviewDialog.h \
	ArcFaceDetection/UiStyle.h

SOURCES += \
	ArcFaceDetection/ArcFaceDetection.cpp \
	ArcFaceDetection/FaceEngine.cpp \
	ArcFaceDetection/IrPreviewDialog.cpp \
        ArcFaceDetection/main.cpp \
        ArcFaceDetection/Utils.cpp

FORMS += \
    ArcFaceDetection/ArcFaceDetection.ui

INCLUDEPATH += /home/yc/work/arcsoft_sdk/inc
INCLUDEPATH += /usr/local/include

LIBS += -L/home/yc/work/arcsoft_sdk/lib/linux_x64 \
    -larcsoft_face_engine \
    -larcsoft_face

LIBS += "-L/usr/local/lib" \
    -lopencv_imgcodecs \
    -lopencv_imgproc \
    -lopencv_core \
    -lopencv_videoio

$QMAKE_CXXFLAGS += pkg-config opencv --libs --cflags opencv

RESOURCES += \
    ArcFaceDetection/resources.qrc

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
