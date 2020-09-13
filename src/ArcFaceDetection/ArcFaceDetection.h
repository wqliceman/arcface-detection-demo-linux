#pragma once
#include <QtWidgets/QMainWindow>
#include "ui_ArcFaceDetection.h"
#include "opencv/cv.h"
#include "opencv/highgui.h"
#include "Utils.h"
#include "FaceEngine.h"
#include "IrPreviewDialog.h"
//#include <QTextCodec>

#include <opencv2/video.hpp>
#include <opencv2/videoio.hpp>

class ArcFaceDetection : public QMainWindow, public Ui_ArcFaceDetectionClass
	, public Utils
{
	Q_OBJECT

public:
	ArcFaceDetection(QWidget *parent = Q_NULLPTR);
	~ArcFaceDetection();

private:
    Ui::ArcFaceDetectionClass ui;

signals:
    void sendMessage(QString msg);

public slots:
    void recvMessage(QString msg);

private:
	float m_rgbLiveThreshold;
	float m_irLiveThreshold;
	float m_rgbFqThreshold;
	int m_rgbCameraId;
	int m_irCameraId;
	int m_compareModel;

    vector<int> m_cameraNameList;
	cv::Mat m_rgbFrame;
	cv::VideoCapture m_rgbCapture;
	cv::Mat m_irFrame;
	cv::VideoCapture m_irCapture;

	bool m_isOpenCamera;

	IrPreviewDialog* irPreviewDialog;
	QStringList picPathList;

	FaceEngine m_imageEngine;
	FaceEngine m_videoEngine;

	int m_nIndex;
	std::vector<ASF_FaceFeature> m_featuresVec;

	ASF_SingleFaceInfo m_recognitionFaceInfo;

	ASF_FaceFeature m_recognitionImageFeature;
	bool m_recognitionImageSucceed;

	ASF_MultiFaceInfo m_videoFaceInfo;
	bool m_dataValid;
	QString m_strCompareInfo;
	QString m_rgbLivenessInfo;
	QString m_irLivenessInfo;

	QPixmap m_pixRgbImage;			//视频模式下预览数据
	QPixmap m_pixStaticRgbImage;	//静态图预览数据

	//QTextCodec *m_textCodec;

	int m_lastFaceId;
	bool m_isLive;
	bool m_isCompareSuccessed;

private:
	void initView();
	void initData();
	void setUiStyle();
	void signalConnectSlot();
	void releaseData();
	void openCamera();
	void closeCamera();
	void editOutputLog(QString str);
	void detectionRecognitionImage(IplImage* recognitionImage, ASF_SingleFaceInfo& faceInfo, QString& strInfo);
	void scaleFaceRect(MRECT srcFaceRect, MRECT* dstFaceRect, double nScale);
	void previewIrFaceRect(MRECT srcFaceRect, MRECT* dstFaceRect);

	//线程函数
	void ftPreviewData();
	void frPreviewData();
	void livenessPreviewData();
	void showRegisterThumbnail();

protected:
	bool m_isPressed;
	QPoint m_movePoint;
	void paintEvent(QPaintEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);
	void mousePressEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);

private slots:
	void operationCamera();				//启动摄像头
	void registerSingleFace();			//注册单张人脸
	void registerFaceDatebase();		//注册人脸库
	void clearFaceDatebase();			//清除人脸库
	void selectRecognitionImage();		//选择识别照
	void faceCompare();					//人脸比对
	
};



