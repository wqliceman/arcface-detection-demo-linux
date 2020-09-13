#include "ArcFaceDetection.h"
#include <QDebug>
#include <QMessageBox>
#include <QPainter>
#include <thread>
#include <mutex>
#include <QFileDialog>
#include <QRegExp>
//#include <windows.h>
#include <QVBoxLayout>
#include "config.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

#define SafeFree(p) { if ((p)) free(p); (p) = NULL; }
#define SafeArrayDelete(p) { if ((p)) delete [] (p); (p) = NULL; } 
#define SafeDelete(p) { if ((p)) delete (p); (p) = NULL; } 

namespace
{

//QString imgFilePath = QFileDialog::getOpenFileName(this, QStringLiteral("选择图片"),
//    "F:\\", tr("Image files(*.bmp *.jpg *.png);;All files (*.*)"));

auto getOpenFileName(
    QWidget *       parent,
    QString const & caption,
    QString const & dir,
    QString const & filter
) -> QString
{
    //parent->hide();
    auto name = QFileDialog::getOpenFileName(
        parent, caption, dir, filter, nullptr,
        QFileDialog::DontUseNativeDialog
    );
    //parent->show();
    return name;
}

}   // namespace

std::mutex g_mtx;

#include <iostream>

ArcFaceDetection::ArcFaceDetection(QWidget *parent)
	: QMainWindow(parent)
{
	setupUi(this);

    signalConnectSlot();

	initView();
	initData();
    setUiStyle();
}

ArcFaceDetection::~ArcFaceDetection()
{ }


void ArcFaceDetection::initView()
{
    //设置取值范围[0,1]，保留两位小数
    QRegExp rx("\\b(0(\\.[0-9][0-9]\\d{0,0})?)|(1(\\.[0][0]\\d{0,0})?)\\b");
    QValidator *validator = new QRegExpValidator(rx);
    editThreshold->setValidator(validator);
    editThreshold->setText(THRESHOLD);
}

void ArcFaceDetection::initData()
{
	m_isPressed = true;
	m_lastFaceId = -1;
	m_isLive = false;
	m_isCompareSuccessed = false;
	m_nIndex = 1;
	m_isOpenCamera = false;
	m_recognitionImageSucceed = false;
	m_dataValid = false;
	m_strCompareInfo = "";
	m_rgbLivenessInfo = "";
	m_irLivenessInfo = "";

	m_recognitionImageFeature.featureSize = FACE_FEATURE_SIZE;
	m_recognitionImageFeature.feature = (MByte *)malloc(m_recognitionImageFeature.featureSize * sizeof(MByte));

	m_videoFaceInfo.faceRect = (MRECT*)malloc(sizeof(MRECT) * FACENUM);
	m_videoFaceInfo.faceID = (MInt32*)malloc(sizeof(MInt32) * FACENUM);
	m_videoFaceInfo.faceOrient = (MInt32*)malloc(sizeof(MInt32) * FACENUM);

	m_featuresVec.clear();

	m_cameraNameList.clear();
	listDevices(m_cameraNameList);

	//m_textCodec = QTextCodec::codecForName("GBK");

    char appId[64];
    char sdkKey[64];
	char activeKey[20];
    readSetting(
        appId, sdkKey, activeKey,
        m_rgbLiveThreshold, m_irLiveThreshold, m_rgbFqThreshold,
        m_rgbCameraId, m_irCameraId, m_compareModel
    );

	MRESULT res = m_videoEngine.ActiveSDK(appId, sdkKey, activeKey);
	if (MOK == res)
	{
        editOutputLog(QStringLiteral(u"SDK激活成功！"));

		res = m_imageEngine.InitEngine(ASF_DETECT_MODE_IMAGE, ASF_OP_ALL_OUT);
		editOutputLog(QStringLiteral("IMAGE模式引擎初始化：") + QString::number(res));

		res = m_videoEngine.InitEngine(ASF_DETECT_MODE_VIDEO, ASF_OP_0_ONLY);
		editOutputLog(QStringLiteral("VIDEO模式引擎初始化：") + QString::number(res));
	}	
	else
	{
        editOutputLog(QStringLiteral(u"SDK激活失败：") + QString::number(res));
	}
}

void ArcFaceDetection::setUiStyle()
{
	setWindowIcon(QIcon(":/Resources/favicon.ico"));
	//setWindowTitle("Arcsoft-ArcFaceDemo c++");
}


void ArcFaceDetection::signalConnectSlot()
{
	connect(btnOperationCamera, &QPushButton::clicked, this, &ArcFaceDetection::operationCamera);
	connect(btnRegister, &QPushButton::clicked, this, &ArcFaceDetection::registerFaceDatebase);
	connect(btnOneRegister, &QPushButton::clicked, this, &ArcFaceDetection::registerSingleFace);
	connect(btnSelectImage, &QPushButton::clicked, this, &ArcFaceDetection::selectRecognitionImage);
	connect(btnClear, &QPushButton::clicked, this, &ArcFaceDetection::clearFaceDatebase);
	connect(btnFaceCompare, &QPushButton::clicked, this, &ArcFaceDetection::faceCompare);

    connect(this, &ArcFaceDetection::sendMessage, this, &ArcFaceDetection::recvMessage, Qt::QueuedConnection);
}

void ArcFaceDetection::releaseData()
{
	SafeFree(m_recognitionImageFeature.feature);
	SafeFree(m_videoFaceInfo.faceRect);
	SafeFree(m_videoFaceInfo.faceID);
	SafeFree(m_videoFaceInfo.faceOrient);
}

// 摄像头模块
void ArcFaceDetection::openCamera()
{
    if (m_cameraNameList.empty())
    {
        QMessageBox::about(NULL, QStringLiteral("提示"), QStringLiteral("摄像头数量不支持！"));
        return;
    }
    if (m_rgbCameraId < 0)
    {
        auto iter = std::find_if(
            m_cameraNameList.cbegin(), m_cameraNameList.cend(),
            [this](int v) { return v != m_irCameraId; }
        );
        if (iter == m_cameraNameList.cend())
        {
            QMessageBox::about(NULL, QStringLiteral(u"提示"), QStringLiteral(u"没有找到 RGB 摄像头！"));
            return;
        }
        m_rgbCameraId = *iter;
    }
    if (1 < m_cameraNameList.size() && m_irCameraId < 0)
    {
        auto iter = std::find_if(
            m_cameraNameList.cbegin(), m_cameraNameList.cend(),
            [this](int v) { return v != m_rgbCameraId; }
        );
        if (iter != m_cameraNameList.cend())
        {
            m_irCameraId = *iter;
        }
    }

    if (0 <= m_rgbCameraId)
    {
        if (!m_rgbCapture.open(m_rgbCameraId))
        {
            QMessageBox::about(NULL, QStringLiteral("提示"), QStringLiteral("RGB摄像头索引配置不正确！"));
            return;
        }
        if (!(m_rgbCapture.set(CV_CAP_PROP_FRAME_WIDTH, VIDEO_FRAME_DEFAULT_WIDTH) &&
            m_rgbCapture.set(CV_CAP_PROP_FRAME_HEIGHT, VIDEO_FRAME_DEFAULT_HEIGHT)))
        {
            QMessageBox::about(NULL, QStringLiteral("提示"), QStringLiteral("RGB摄像头初始化失败！"));
            return;
        }
        m_isOpenCamera = true;
    }

    if (0 <= m_irCameraId)
    {
        if (!m_irCapture.open(m_irCameraId))
        {
            QMessageBox::about(NULL, QStringLiteral("提示"), QStringLiteral("IR摄像头被占用！"));
            return;
        }
        if (!(m_irCapture.set(CV_CAP_PROP_FRAME_WIDTH, VIDEO_FRAME_DEFAULT_WIDTH) &&
            m_irCapture.set(CV_CAP_PROP_FRAME_HEIGHT, VIDEO_FRAME_DEFAULT_HEIGHT)))
        {
            QMessageBox::about(NULL, QStringLiteral("提示"), QStringLiteral("IR摄像头初始化失败！"));
            return;
        }
        m_isOpenCamera = true;
    }
}

void ArcFaceDetection::closeCamera()
{
	m_isOpenCamera = false;
	m_dataValid = false;
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
//	Sleep(600);
}

void ArcFaceDetection::operationCamera()
{
	if (btnOperationCamera->text() == QStringLiteral("启动摄像头"))
	{
		openCamera();
	}
	else
	{
		closeCamera();
	}
	
	if (m_isOpenCamera)
	{
		btnOperationCamera->setText(QStringLiteral("关闭摄像头"));

		std::thread thFt(&ArcFaceDetection::ftPreviewData, this);
		thFt.detach();

		std::thread thFr(&ArcFaceDetection::frPreviewData, this);
		thFr.detach();

		std::thread thLiveness(&ArcFaceDetection::livenessPreviewData, this);
		thLiveness.detach();

		if (2 == m_cameraNameList.size())
		{
			irPreviewDialog = new IrPreviewDialog();
			irPreviewDialog->show();
		}
	}
	else 
	{
		btnOperationCamera->setText(QStringLiteral("启动摄像头"));
		if (2 == m_cameraNameList.size())
		{
			delete irPreviewDialog;
			irPreviewDialog = nullptr;
		}
	}
}

//ft预览
void ArcFaceDetection::ftPreviewData()
{
	MRESULT res = MOK;
	MRECT* dstFaceRect = (MRECT*)malloc(sizeof(MRECT));
	
	while (m_isOpenCamera)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        //Sleep(1);
		//处理RGB摄像头数据
		m_rgbCapture >> m_rgbFrame;
		if (!m_rgbFrame.empty())
		{
			IplImage *rgbImage = NULL;
			{
				std::lock_guard<mutex> lock(g_mtx);
                IplImage iplrgbImage = cvIplImage(m_rgbFrame);
				rgbImage = cvCloneImage(&iplrgbImage);
			}

			res = m_videoEngine.PreDetectFace(rgbImage, m_videoFaceInfo, true);
			if (MOK == res)
			{
                m_dataValid = true;
				//画框
				cvRectangle(rgbImage, cvPoint(m_videoFaceInfo.faceRect[0].left, m_videoFaceInfo.faceRect[0].top),
					cvPoint(m_videoFaceInfo.faceRect[0].right, m_videoFaceInfo.faceRect[0].bottom), 
					(m_isCompareSuccessed && m_isLive) ? cvScalar(0, 255, 0) : cvScalar(0, 0, 255), 2);

				//画字
				CvFont font;
				cvInitFont(&font, CV_FONT_HERSHEY_COMPLEX, 0.5, 0.8, 0);
				cvPutText(rgbImage, m_strCompareInfo.toStdString().c_str(),
					cvPoint(m_videoFaceInfo.faceRect[0].left, m_videoFaceInfo.faceRect[0].top - 15), &font, 
					(m_isCompareSuccessed && m_isLive) ? cvScalar(0, 255, 0) : cvScalar(0, 0, 255));

				cvPutText(rgbImage, m_rgbLivenessInfo.toStdString().c_str(),
					cvPoint(m_videoFaceInfo.faceRect[0].left, m_videoFaceInfo.faceRect[0].top + 20), &font, 
					(m_isCompareSuccessed && m_isLive) ? cvScalar(0, 255, 0) : cvScalar(0, 0, 255));
			} else {
				m_dataValid = false;
			}

			m_pixRgbImage = QPixmap::fromImage(cvMatToQImage(cv::cvarrToMat(rgbImage)));
			cvReleaseImage(&rgbImage);
		}

		//处理红外摄像头数据
		if (2 == m_cameraNameList.size())
		{
			m_irCapture >> m_irFrame;
			if (!m_irFrame.empty())
			{
				IplImage *destIrImage = NULL;
				{
					std::lock_guard<mutex> lock(g_mtx);
                    IplImage iplIrImage = cvIplImage(m_irFrame);

					//图像缩放
					CvSize destImgSize;
					destImgSize.width = iplIrImage.width / 2;
					destImgSize.height = iplIrImage.height / 2;
					destIrImage = cvCreateImage(destImgSize, iplIrImage.depth, iplIrImage.nChannels);
					cvResize(&iplIrImage, destIrImage, CV_INTER_AREA);
				}

				if (MOK == res)
				{
					//画框
					scaleFaceRect(m_videoFaceInfo.faceRect[0], dstFaceRect, 1.0);
					previewIrFaceRect(*dstFaceRect, dstFaceRect);	//预览人脸框边界值保护
					scaleFaceRect(*dstFaceRect, dstFaceRect, 0.5);

					cvRectangle(destIrImage, cvPoint(dstFaceRect->left, dstFaceRect->top),
						cvPoint(dstFaceRect->right, dstFaceRect->bottom), 
						(m_isCompareSuccessed && m_isLive) ? cvScalar(0, 255, 0) : cvScalar(0, 0, 255), 1);

					//画字
					CvFont font;
					cvInitFont(&font, CV_FONT_HERSHEY_COMPLEX, 0.5, 0.5, 0);
					cvPutText(destIrImage, m_irLivenessInfo.toStdString().c_str(),
						cvPoint(dstFaceRect->left, dstFaceRect->top - 15), &font, 
						(m_isCompareSuccessed && m_isLive) ? cvScalar(0, 255, 0) : cvScalar(0, 0, 255));
				}

				irPreviewDialog->m_pixIrImage = QPixmap::fromImage(cvMatToQImage(cv::cvarrToMat(destIrImage)));
				cvReleaseImage(&destIrImage);
			}
		}
	}

	SafeFree(dstFaceRect);
	labelPreview->clear();
	m_rgbCapture.release();
	if (2 == m_cameraNameList.size())
	{
		m_irCapture.release();
	}
}

//特征提取、比对
void ArcFaceDetection::frPreviewData()
{
	ASF_SingleFaceInfo singleFaceInfo = { 0 };
	ASF_FaceFeature faceFeature = { 0 };
	faceFeature.featureSize = FACE_FEATURE_SIZE;
	faceFeature.feature = (MByte *)malloc(faceFeature.featureSize * sizeof(MByte));
	
	while (m_isOpenCamera)
	{
		if (!m_dataValid)
		{
			continue;
		}

		//与上一张人脸比较若不是同一张人脸，人脸比对和活体检测数据清零
		if (m_lastFaceId != m_videoFaceInfo.faceID[0])
		{
			m_strCompareInfo = "";
			m_isCompareSuccessed = false;
			m_isLive = false;
			m_rgbLivenessInfo = "";
			m_irLivenessInfo = "";
		}

		m_lastFaceId = m_videoFaceInfo.faceID[0];

		if (m_isCompareSuccessed)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(3));
            //Sleep(3);
			continue;
		}

		IplImage *rgbImage = NULL;
		{
			std::lock_guard<mutex> lock(g_mtx);
			if (m_rgbFrame.empty())
			{
				continue;
			}
            IplImage iplRgbImage = cvIplImage(m_rgbFrame);
			rgbImage = cvCloneImage(&iplRgbImage);

			singleFaceInfo.faceOrient = m_videoFaceInfo.faceOrient[0];
			singleFaceInfo.faceRect = m_videoFaceInfo.faceRect[0];
		}

        MRESULT res = m_videoEngine.PreExtractFeature(rgbImage, singleFaceInfo, faceFeature);
		if (MOK != res || m_featuresVec.size() <= 0)
		{
			cvReleaseImage(&rgbImage);
			continue;
		}

		int maxIndex = 0, curIndex = 0;
		MFloat maxThreshold = 0.0;
		for (auto regisFeature : m_featuresVec)
		{
			curIndex++;
			MFloat confidenceLevel = 0.0;
			res = m_videoEngine.FacePairMatching(confidenceLevel, faceFeature, regisFeature, 
				ASF_CompareModel(m_compareModel));
			if (MOK == res && confidenceLevel > maxThreshold)
			{
				maxThreshold = confidenceLevel;
				maxIndex = curIndex;
			}
		}

		if (maxThreshold > editThreshold->text().toDouble())
		{
			m_isCompareSuccessed = true;
			m_strCompareInfo = "ID:" + QString::number(maxIndex) + " Threshold:" + QString("%1").arg(maxThreshold);
		}
		else
		{
			m_strCompareInfo = "";
		}
		cvReleaseImage(&rgbImage);
	}
	SafeFree(faceFeature.feature);
}

//活体检测
void ArcFaceDetection::livenessPreviewData()
{
	ASF_MultiFaceInfo multiFaceInfo = { 0 };
	multiFaceInfo.faceOrient = (MInt32*)malloc(sizeof(MInt32) * FACENUM);
	multiFaceInfo.faceRect = (MRECT*)malloc(sizeof(MRECT) * FACENUM);
	multiFaceInfo.faceID = (MInt32*)malloc(sizeof(MInt32) * FACENUM);

	while (m_isOpenCamera)
	{
		if (!m_dataValid)
		{
            std::this_thread::sleep_for(std::chrono::milliseconds(3));
            // Sleep(3);	//无人脸避免持续循环，浪费cpu资源
			continue;
		}

		MRESULT res = MOK;
		{
			std::lock_guard<mutex> lock(g_mtx);
			multiFaceInfo.faceNum = 1;
			multiFaceInfo.faceOrient[0] = m_videoFaceInfo.faceOrient[0];
			multiFaceInfo.faceRect[0] = m_videoFaceInfo.faceRect[0];
			multiFaceInfo.faceID[0] = m_videoFaceInfo.faceID[0];
		}

		// 活体检测为真时本张人脸不再处理
		if (m_isLive)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(600));
            //Sleep(3);
			continue;
		}

		//单目默认只做RGB活体，使用RGB活体检测结果去判断人脸比对是否通过
		if (1 == m_cameraNameList.size())
		{
			//RGB活体检测
			IplImage *rgbImage = NULL;
			{
				std::lock_guard<mutex> lock(g_mtx);
				if (m_rgbFrame.empty())
				{
					continue;
				}
                IplImage iplRgbImage = cvIplImage(m_rgbFrame);
				rgbImage = cvCloneImage(&iplRgbImage);
			}

			ASF_LivenessInfo rgbLivenessInfo = { 0 };
			res = m_imageEngine.livenessProcess(multiFaceInfo, rgbImage, rgbLivenessInfo, true);
			if (MOK == res && rgbLivenessInfo.num > 0 && rgbLivenessInfo.isLive[0] == 1)
			{
				m_rgbLivenessInfo = "Live:Live";
				m_isLive = true;
			}
			else
			{
				if (MOK != res || rgbLivenessInfo.num < 1)
				{
					m_rgbLivenessInfo = "";
				}
				else
				{
					rgbLivenessInfo.isLive[0] == 0 ? m_rgbLivenessInfo = "Live:No Live" : m_rgbLivenessInfo = "Live:Unknown";
				}
			}
			cvReleaseImage(&rgbImage);
		}

		//双目只做IR活体，人脸比对使用IR活体去判断人脸比对是否通过
		if (2 == m_cameraNameList.size())
		{
			IplImage *irImage = NULL;
			{
				
				std::lock_guard<mutex> lock(g_mtx);
				if (m_irFrame.empty())
				{
					continue;
				}
                IplImage iplIrImage = cvIplImage(m_irFrame);
				irImage = cvCloneImage(&iplIrImage);
			}

			//双目摄像头偏移量调整
			multiFaceInfo.faceRect[0].left -= BINOCULAR_CAMERA_OFFSET_LEFT_RIGHT;
			multiFaceInfo.faceRect[0].top -= BINOCULAR_CAMERA_OFFSET_TOP_BOTTOM;
			multiFaceInfo.faceRect[0].right -= BINOCULAR_CAMERA_OFFSET_LEFT_RIGHT;
			multiFaceInfo.faceRect[0].bottom -= BINOCULAR_CAMERA_OFFSET_TOP_BOTTOM;

			ASF_LivenessInfo irLivenessInfo = { 0 };
			res = m_imageEngine.livenessProcess(multiFaceInfo, irImage, irLivenessInfo, false);
			if (MOK == res && irLivenessInfo.num > 0 && irLivenessInfo.isLive[0] == 1)
			{
				m_irLivenessInfo = "Live:Live";
				m_isLive = true;
			}
			else
			{
				if (MOK != res || irLivenessInfo.num < 1)
				{
					m_irLivenessInfo = "";
				}
				else
				{
					irLivenessInfo.isLive[0] == 0 ? m_irLivenessInfo = "Live:No Live" : m_irLivenessInfo = "Live:Unknown";
				}
			}
			cvReleaseImage(&irImage);
		}
	}

	SafeFree(multiFaceInfo.faceOrient);
	SafeFree(multiFaceInfo.faceRect);
	SafeFree(multiFaceInfo.faceID);
}



//红外预览人脸框边界值保护
void ArcFaceDetection::previewIrFaceRect(MRECT srcFaceRect, MRECT* dstFaceRect)
{
	dstFaceRect->left = srcFaceRect.left - BINOCULAR_CAMERA_OFFSET_LEFT_RIGHT;
	if (dstFaceRect->left < 0)
		dstFaceRect->left = 2;
	dstFaceRect->top = srcFaceRect.top - BINOCULAR_CAMERA_OFFSET_TOP_BOTTOM;
	if (dstFaceRect->top < 0)
		dstFaceRect->top = 2;
	dstFaceRect->right = srcFaceRect.right - BINOCULAR_CAMERA_OFFSET_LEFT_RIGHT;
	if (dstFaceRect->right > VIDEO_FRAME_DEFAULT_WIDTH)
		dstFaceRect->right = VIDEO_FRAME_DEFAULT_WIDTH - 2;
	dstFaceRect->bottom = srcFaceRect.bottom - BINOCULAR_CAMERA_OFFSET_TOP_BOTTOM;
	if (dstFaceRect->bottom > VIDEO_FRAME_DEFAULT_HEIGHT)
		dstFaceRect->bottom = VIDEO_FRAME_DEFAULT_HEIGHT - 2;
}

//注册单张人脸
void ArcFaceDetection::registerSingleFace()
{
	registerListWidget->setIconSize(QSize(LIST_WIDGET_ITEM_WIDTH, LIST_WIDGET_ITEM_HEIGHT));
	//设置QListWidget的显示模式
	registerListWidget->setViewMode(QListView::IconMode);
	//设置QListWidget中的单元项不可被拖动
	registerListWidget->setMovement(QListView::Static);
	registerListWidget->setSpacing(13);

    picPathList.clear();
    auto imgFilePath = getOpenFileName(this, QStringLiteral("选择图片"), "/home", tr("Image files(*.bmp *.jpg *.png);;All files (*.*)"));
    // QString imgFilePath = QFileDialog::getOpenFileName(this, QStringLiteral("选择图片"), "F:\\", tr("Image files(*.bmp *.jpg *.png);;All files (*.*)"));

	if (imgFilePath.isEmpty())
	{
		QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请选择注册照!"));
		return;
	}

	picPathList << imgFilePath;	//插入链表

	if (picPathList.size() > 0)
	{
		std::thread th(&ArcFaceDetection::showRegisterThumbnail, this);
		th.detach();
	}
}

// 人脸库操作
void ArcFaceDetection::registerFaceDatebase()
{
    registerListWidget->setIconSize(QSize(LIST_WIDGET_ITEM_WIDTH, LIST_WIDGET_ITEM_HEIGHT));
    //设置QListWidget的显示模式
    registerListWidget->setViewMode(QListView::IconMode);
    //设置QListWidget中的单元项不可被拖动
    registerListWidget->setMovement(QListView::Static);
    registerListWidget->setSpacing(13);

    picPathList.clear();
    picPathList = getDestFolderImages();

    if (picPathList.size() > 0)
    {
        std::thread th(&ArcFaceDetection::showRegisterThumbnail, this);
        th.detach();
    }
    qDebug() << picPathList;
}

void ArcFaceDetection::showRegisterThumbnail()
{
    editOutputLog(QStringLiteral("开始注册人脸库..."));
    editOutputLog(QStringLiteral("选择待注册图：") + QString::number(picPathList.size()));

	ASF_MultiFaceInfo multiFaceInfo = { 0 };
	multiFaceInfo.faceOrient = (MInt32*)malloc(sizeof(MInt32) * FACENUM);	//一张人脸
	multiFaceInfo.faceRect = (MRECT*)malloc(sizeof(MRECT) * FACENUM);

    qDebug() << picPathList;
	for (int i = 0; i < picPathList.size(); ++i)
	{
        qDebug() << i;
		QString picPath = picPathList.at(i);
		std::string strPicPath = picPath.toStdString();
		strPicPath = (const char*)picPath.toLocal8Bit();	//防止中文乱码

		IplImage* registerImage = cvLoadImage(strPicPath.c_str());
		MRESULT res = m_imageEngine.PreDetectFace(registerImage, multiFaceInfo, true);
		if (MOK != res)
		{
			cvReleaseImage(&registerImage);
			continue;
		}

		ASF_SingleFaceInfo singleFaceInfo = { 0 };
		singleFaceInfo.faceOrient = multiFaceInfo.faceOrient[0];
		singleFaceInfo.faceRect = multiFaceInfo.faceRect[0];

		ASF_FaceFeature faceFeature = { 0 };
		faceFeature.featureSize = FACE_FEATURE_SIZE;
		faceFeature.feature = (MByte *)malloc(faceFeature.featureSize * sizeof(MByte));
        res = m_imageEngine.PreExtractFeature(registerImage, singleFaceInfo, faceFeature);

		if (MOK != res)
		{
			SafeFree(faceFeature.feature);
			cvReleaseImage(&registerImage);
			continue;
		}

		//将图片数据加入到内存
		QByteArray pData;
		QFile fp(picPath);
		fp.open(QIODevice::ReadOnly);
		pData = fp.readAll();
		fp.close();
		QImage imgPix;
		imgPix.loadFromData(pData);
		QPixmap pix;
		pix = QPixmap::fromImage(imgPix);
		
		QListWidgetItem *pItem = new QListWidgetItem(QIcon(pix.scaled(QSize(LIST_WIDGET_ITEM_WIDTH, LIST_WIDGET_ITEM_HEIGHT), Qt::KeepAspectRatio)), 
			QString::number(m_nIndex) + QStringLiteral("号"), registerListWidget);
		//设置单元项的宽度和高度
		pItem->setSizeHint(QSize(LIST_WIDGET_ITEM_WIDTH, LIST_WIDGET_ITEM_HEIGHT + 20));
		pItem->setTextAlignment(Qt::AlignHCenter | Qt::AlignBottom);

		m_featuresVec.push_back(faceFeature);
		cvReleaseImage(&registerImage);
		m_nIndex++;
	}
    qDebug();
    editOutputLog(QStringLiteral("注册成功：") + QString::number(m_nIndex - 1));

    SafeFree(multiFaceInfo.faceOrient);
    SafeFree(multiFaceInfo.faceRect);
    qDebug();
}

void ArcFaceDetection::clearFaceDatebase()
{
	if (m_isOpenCamera)
	{
		QMessageBox::about(NULL, QStringLiteral("提示"), QStringLiteral("请先关闭摄像头！"));
		return;
	}

	m_nIndex = 1;
	registerListWidget->clear();

	for (auto feature : m_featuresVec)
	{
		SafeFree(feature.feature);
	}

	m_featuresVec.clear();

	editOutputLog(QStringLiteral("清除人脸库"));
}


// 识别照操作
void ArcFaceDetection::selectRecognitionImage()
{
	if (m_isOpenCamera)
	{
		QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先关闭摄像头!"));
		return;
	}

    auto imgFilePath = getOpenFileName(this, "Select Image", "/home", "Image files(*.bmp *.jpg *.png);;All files (*.*)");
//    QString imgFilePath = QFileDialog::getOpenFileName(
//        this,
//        "Select Image",
//        "/home",
//        "Image files(*.bmp *.jpg *.png);;All files (*.*)"
//    );

	if (imgFilePath.isEmpty())
	{
		QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请选择识别照!"));
		return;
	}

    // FD、FR、Process 算法处理
	QString strProcessInfo;
	std::string strPicPath = string(imgFilePath.toLocal8Bit());
	IplImage* recognitionImage = cvLoadImage(strPicPath.c_str());
    detectionRecognitionImage(recognitionImage, m_recognitionFaceInfo, strProcessInfo);
    // 缩放比例
	double nScale = VIDEO_FRAME_DEFAULT_WIDTH / VIDEO_FRAME_DEFAULT_HEIGHT;
	if (double(recognitionImage->width) / double(recognitionImage->height) > nScale)
	{
		nScale = VIDEO_FRAME_DEFAULT_WIDTH / double(recognitionImage->width);
	}
	else
	{
		nScale = VIDEO_FRAME_DEFAULT_HEIGHT / double(recognitionImage->height);
	}

    // 对图片进行等比例缩放
	CvSize destImgSize;
	destImgSize.width = recognitionImage->width * nScale;
	destImgSize.height = recognitionImage->height * nScale;
	IplImage *destImage = cvCreateImage(destImgSize, recognitionImage->depth, recognitionImage->nChannels);
	cvResize(recognitionImage, destImage, CV_INTER_AREA);

    // 对人脸框进行按比例缩放
	MRECT* destRect = (MRECT*)malloc(sizeof(MRECT));
	scaleFaceRect(m_recognitionFaceInfo.faceRect, destRect, nScale);

    // 画框
    cvRectangle(
        destImage,
        cvPoint(destRect->left, destRect->top),
        cvPoint(destRect->right, destRect->bottom),
        cvScalar(100, 200, 255),
        2, 1, 0
    );

    // 画字
	CvFont font;
	cvInitFont(&font, CV_FONT_HERSHEY_COMPLEX, 0.5, 0.8, 0);
	cvPutText(destImage, strProcessInfo.toStdString().c_str(),
		cvPoint(destRect->left, destRect->top - 15), &font, cvScalar(90, 240, 120));

    // 预览
	m_pixStaticRgbImage = QPixmap::fromImage(cvMatToQImage(cv::cvarrToMat(destImage)));
	
	
	SafeFree(destRect);
	cvReleaseImage(&recognitionImage);
	cvReleaseImage(&destImage);
}

void ArcFaceDetection::detectionRecognitionImage(IplImage* recognitionImage, ASF_SingleFaceInfo& faceInfo, QString& strInfo)
{
	ASF_MultiFaceInfo multiFaceInfo = { 0 };
	multiFaceInfo.faceOrient = (MInt32*)malloc(sizeof(MInt32) * FACENUM);	//一张人脸
	multiFaceInfo.faceRect = (MRECT*)malloc(sizeof(MRECT) * FACENUM);
	MRESULT res = m_imageEngine.PreDetectFace(recognitionImage, multiFaceInfo, true);
	if (MOK != res)
	{
		editOutputLog(QStringLiteral("未检测到人脸！"));
	}
	else
	{
		//图像质量检测，默认阈值为0.35
        if (true)
		{
			ASF_AgeInfo ageInfo = { 0 };
			ASF_GenderInfo genderInfo = { 0 };
			ASF_Face3DAngle angleInfo = { 0 };
			ASF_LivenessInfo livenessInfo = { 0 };

			//赋值传出
			faceInfo.faceOrient = multiFaceInfo.faceOrient[0];
			faceInfo.faceRect = multiFaceInfo.faceRect[0];

			res = m_imageEngine.FaceProcess(multiFaceInfo, recognitionImage, ageInfo, genderInfo, angleInfo, livenessInfo);
			if (MOK == res)
			{
				strInfo.sprintf("%s, %s, %s", to_string(ageInfo.ageArray[0]).c_str(),
					genderInfo.genderArray[0] == 0 ? "Male" : (genderInfo.genderArray[0] == 1 ? "Female" : "Unknown"),
					livenessInfo.isLive[0] == 1 ? "Live" : (livenessInfo.isLive[0] == 0 ? "NoLive" : "Unknown"));
			}

			//FR
            res = m_imageEngine.PreExtractFeature(recognitionImage, faceInfo, m_recognitionImageFeature);
			if (MOK == res)
			{
				editOutputLog(QStringLiteral("识别照特征提取成功！"));
                m_recognitionImageSucceed = true;
			}
			else
			{
				editOutputLog(QStringLiteral("识别照特征提取失败！"));
                m_recognitionImageSucceed = false;
			}
		}
		else
		{
			editOutputLog(QStringLiteral("图像质量不合格！"));
		}
	}
	SafeFree(multiFaceInfo.faceOrient);
	SafeFree(multiFaceInfo.faceRect);
}

void ArcFaceDetection::scaleFaceRect(MRECT srcFaceRect, MRECT* dstFaceRect, double nScale)
{
	dstFaceRect->left = srcFaceRect.left * nScale;
	dstFaceRect->top = srcFaceRect.top * nScale;
	dstFaceRect->right = srcFaceRect.right * nScale;
	dstFaceRect->bottom = srcFaceRect.bottom * nScale;
}

//人脸比对模块
void ArcFaceDetection::faceCompare()
{
	if (!m_recognitionImageSucceed)
	{
		QMessageBox::about(NULL, QStringLiteral("提示"), QStringLiteral("人脸比对失败，请选择识别照！"));
		return;
	}

	if (m_featuresVec.size() == 0)
	{
		QMessageBox::about(NULL, QStringLiteral("提示"), QStringLiteral("请注册人脸库！"));
		return;
	}

	int maxIndex = 1, curIndex = 0;
	MFloat maxThreshold = 0.0;
	for (auto registerFeature : m_featuresVec)
	{
		curIndex++;
		MFloat confidenceLevel = 0.0;
		MRESULT pairRes = m_imageEngine.FacePairMatching(confidenceLevel, m_recognitionImageFeature, 
			registerFeature, (ASF_CompareModel)m_compareModel);

		if (MOK == pairRes && confidenceLevel > maxThreshold)
		{
			maxThreshold = confidenceLevel;
			maxIndex = curIndex;
		}
	}

	editOutputLog(QString::number(maxIndex) + QStringLiteral("号：") + QString("%1").arg(maxThreshold));
	editOutputLog(QStringLiteral("比对结束！"));
}

//日志输出
void ArcFaceDetection::editOutputLog(QString msg)
{
    emit sendMessage(msg);
}

void ArcFaceDetection::recvMessage(QString msg)
{
    editOutLog->append(msg);
}

//预览绘制
void ArcFaceDetection::paintEvent(QPaintEvent *event)
{
	Q_UNUSED(event);

	if (m_isOpenCamera)
	{
		labelPreview->setPixmap(m_pixRgbImage);
	}
	else
	{
		labelPreview->setPixmap(m_pixStaticRgbImage);
		labelPreview->setAlignment(Qt::AlignCenter);
	}
}

// 无边框移动
void ArcFaceDetection::mouseMoveEvent(QMouseEvent *event)
{
	if (m_isPressed)
		move(event->pos() - m_movePoint + pos());
}

void ArcFaceDetection::mouseReleaseEvent(QMouseEvent *event)
{
	Q_UNUSED(event);
	m_isPressed = false;
}

void ArcFaceDetection::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton)
	{
		m_isPressed = true;
		m_movePoint = event->pos();
	}
}
