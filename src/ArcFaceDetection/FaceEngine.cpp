#include "FaceEngine.h"
#include "config.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>


//#pragma comment(lib, "libarcsoft_face_engine.lib")


FaceEngine::FaceEngine()
{
	initData();
}


FaceEngine::~FaceEngine()
{ }

void FaceEngine::initData()
{ }

MRESULT FaceEngine::ActiveSDK(char* appId, char* sdkKey, char* activeKey)
{
#ifdef ARCFACE_PRO
	MRESULT res = ASFOnlineActivation(appId, sdkKey, activeKey);
#else 
	MRESULT res = ASFOnlineActivation(appId, sdkKey);
#endif
	if (MOK != res && MERR_ASF_ALREADY_ACTIVATED != res)
		return res;
	return MOK;
}

MRESULT FaceEngine::GetActiveFileInfo(ASF_ActiveFileInfo& activeFileInfo)
{
	MRESULT res = ASFGetActiveFileInfo(&activeFileInfo);
	return res;
}

MRESULT FaceEngine::InitEngine(ASF_DetectMode detectMode, ASF_OrientPriority detectFaceOrientPriority)
{
	m_hEngine = NULL;
	MInt32 mask = 0;

	if (ASF_DETECT_MODE_IMAGE == detectMode)
	{
		mask = ASF_FACE_DETECT | ASF_FACERECOGNITION | ASF_AGE | ASF_GENDER | ASF_FACE3DANGLE | 
            ASF_LIVENESS | ASF_IR_LIVENESS;
	}
	else
	{
        mask = ASF_FACE_DETECT | ASF_FACERECOGNITION | ASF_LIVENESS | ASF_IR_LIVENESS;
	}

	MRESULT res = ASFInitEngine(detectMode, detectFaceOrientPriority, NSCALE, FACENUM, mask, &m_hEngine);
	return res;
}

MRESULT FaceEngine::UnInitEngine()
{
	//销毁引擎
	return ASFUninitEngine(m_hEngine);
}

MRESULT FaceEngine::PreDetectFace(IplImage* image, ASF_MultiFaceInfo& multiFaceInfo, bool isRgb)
{
	if (!image)
	{
		return -1;
	}

	IplImage* cutImg = cvCreateImage(cvSize(image->width - (image->width % 4), image->height),
		IPL_DEPTH_8U, image->nChannels);
	PicCutOut(image, cutImg, 0, 0);

	MRESULT res = MOK;
	ASF_MultiFaceInfo detectedFaces = { 0 };//人脸检测

	if (isRgb)
	{
		ASVLOFFSCREEN rgbOffscreen = { 0 };
		ColorSpaceConversion(ASVL_PAF_RGB24_B8G8R8, cutImg, rgbOffscreen);

		res = ASFDetectFacesEx(m_hEngine, &rgbOffscreen, &detectedFaces);
	}
	else  //IR图像
	{
		cv::Mat grayMat;
        cv::Mat matImg = cv::cvarrToMat(cutImg);						//IplImage转Mat 设为ture为深拷贝
        cv::cvtColor(matImg, grayMat, CV_BGR2GRAY);
        auto temp = cvIplImage(grayMat);
        IplImage* iplGrayMat = &temp;			//mat 转 IplImage 浅拷贝

		ASVLOFFSCREEN grayOffscreen = { 0 };
		ColorSpaceConversion(ASVL_PAF_GRAY, iplGrayMat, grayOffscreen);

		res = ASFDetectFacesEx(m_hEngine, &grayOffscreen, &detectedFaces);
	}

	if (res != MOK || detectedFaces.faceNum < 1)
	{
		cvReleaseImage(&cutImg);
		return -1;
	}

	for (int i = 0; i < detectedFaces.faceNum; i++)
	{
		multiFaceInfo.faceRect[i].left = detectedFaces.faceRect[i].left;
		multiFaceInfo.faceRect[i].top = detectedFaces.faceRect[i].top;
		multiFaceInfo.faceRect[i].right = detectedFaces.faceRect[i].right;
		multiFaceInfo.faceRect[i].bottom = detectedFaces.faceRect[i].bottom;
		multiFaceInfo.faceOrient[i] = detectedFaces.faceOrient[i];
		multiFaceInfo.faceNum = detectedFaces.faceNum;
		if (detectedFaces.faceID != nullptr)
		{
			multiFaceInfo.faceID[i] = detectedFaces.faceID[i];
		}
	}

	cvReleaseImage(&cutImg);
	return res;
}

MRESULT FaceEngine::PreImageQualityDetect(
    IplImage* image,
    ASF_MultiFaceInfo multiFaceInfo,
    float rgbFqThreshold
)
{
	if (!image || image->imageData == NULL)
	{
		return -1;
	}

	IplImage* cutImg = cvCreateImage(cvSize(image->width - (image->width % 4), image->height),
		IPL_DEPTH_8U, image->nChannels);
	PicCutOut(image, cutImg, 0, 0);
	if (!cutImg)
	{
		cvReleaseImage(&cutImg);
		return -1;
	}

	ASVLOFFSCREEN rgbOffscreen = { 0 };
	ColorSpaceConversion(ASVL_PAF_RGB24_B8G8R8, cutImg, rgbOffscreen);
	cvReleaseImage(&cutImg);
    return -1;
}

MRESULT FaceEngine::PreExtractFeature(IplImage* image, ASF_SingleFaceInfo& faceRect, 
    ASF_FaceFeature& feature)
{
	if (!image || image->imageData == NULL)
	{
		return -1;
	}
	IplImage* cutImg = cvCreateImage(cvSize(image->width - (image->width % 4), image->height),
		IPL_DEPTH_8U, image->nChannels);

	PicCutOut(image, cutImg, 0, 0);

	if (!cutImg)
	{
		cvReleaseImage(&cutImg);
		return -1;
	}

	MRESULT res = MOK;

	ASF_FaceFeature detectFaceFeature = { 0 };//特征值
	ASVLOFFSCREEN rgbOffscreen = { 0 };
	ColorSpaceConversion(ASVL_PAF_RGB24_B8G8R8, cutImg, rgbOffscreen);

    res = ASFFaceFeatureExtractEx(
                m_hEngine,
                &rgbOffscreen,
                &faceRect,
                &detectFaceFeature);

	if (MOK != res)
	{
		cvReleaseImage(&cutImg);
		return res;
	}

	if (!feature.feature)
	{
		return -1;
	}
	memset(feature.feature, 0, detectFaceFeature.featureSize);
	memcpy(feature.feature, detectFaceFeature.feature, detectFaceFeature.featureSize);
	cvReleaseImage(&cutImg);

	return res;
}

MRESULT FaceEngine::FacePairMatching(MFloat &confidenceLevel, ASF_FaceFeature feature1, 
	ASF_FaceFeature feature2, ASF_CompareModel compareModel)
{
	MRESULT res = ASFFaceFeatureCompare(m_hEngine, &feature1, &feature2, &confidenceLevel, compareModel);
	return res;
}

MRESULT FaceEngine::SetLivenessThreshold(MFloat	rgbLiveThreshold, MFloat irLiveThreshold)
{
	ASF_LivenessThreshold threshold = { 0 };
	threshold.thresholdmodel_BGR = rgbLiveThreshold;
	threshold.thresholdmodel_IR = irLiveThreshold;

	MRESULT res = ASFSetLivenessParam(m_hEngine, &threshold);
	return res;
}

MRESULT FaceEngine::FaceProcess(ASF_MultiFaceInfo detectedFaces, IplImage *img, ASF_AgeInfo &ageInfo,
	ASF_GenderInfo &genderInfo, ASF_Face3DAngle &angleInfo, ASF_LivenessInfo& liveNessInfo)
{
	if (!img)
	{
		return -1;
	}

	IplImage* cutImg = cvCreateImage(cvSize(img->width - (img->width % 4), img->height), IPL_DEPTH_8U, img->nChannels);
	PicCutOut(img, cutImg, 0, 0);

	if (!cutImg)
	{
		cvReleaseImage(&cutImg);
		return -1;
	}

	ASVLOFFSCREEN rgbOffscreen = { 0 };
	ColorSpaceConversion(ASVL_PAF_RGB24_B8G8R8, cutImg, rgbOffscreen);
	
	MInt32 combinedMask = ASF_AGE | ASF_GENDER | ASF_FACE3DANGLE | ASF_LIVENESS;
	MRESULT res = ASFProcessEx(m_hEngine, &rgbOffscreen, &detectedFaces, combinedMask);

	res = ASFGetAge(m_hEngine, &ageInfo);
	res = ASFGetGender(m_hEngine, &genderInfo);
	res = ASFGetFace3DAngle(m_hEngine, &angleInfo);
	res = ASFGetLivenessScore(m_hEngine, &liveNessInfo);

	cvReleaseImage(&cutImg);
	return res;
}

MRESULT FaceEngine::livenessProcess(ASF_MultiFaceInfo detectedFaces, IplImage *img, ASF_LivenessInfo& livenessInfo, bool isRgb)
{
	if (!img)
	{  
		return -1;
	}

	IplImage* cutImg = cvCreateImage(cvSize(img->width - (img->width % 4), img->height), IPL_DEPTH_8U, img->nChannels);
	PicCutOut(img, cutImg, 0, 0);

	if (!cutImg)
	{
		cvReleaseImage(&cutImg);
		return -1;
	}

	MRESULT res = MOK;
	if (isRgb)
	{
		ASVLOFFSCREEN rgbOffscreen = { 0 };
		ColorSpaceConversion(ASVL_PAF_RGB24_B8G8R8, cutImg, rgbOffscreen);

		res = ASFProcessEx(m_hEngine, &rgbOffscreen, &detectedFaces, ASF_LIVENESS);

		res = ASFGetLivenessScore(m_hEngine, &livenessInfo);
	}
	else
	{
		cv::Mat grayMat;
        cv::Mat matImg = cv::cvarrToMat(cutImg);    		// IplImage转Mat 设为ture为深拷贝
		cv::cvtColor(matImg, grayMat, CV_BGR2GRAY);
        auto temp = cvIplImage(grayMat);

        IplImage* iplGrayMat = &temp;               // mat 转 IplImage 浅拷贝
		ASVLOFFSCREEN grayOffscreen = { 0 };
		ColorSpaceConversion(ASVL_PAF_GRAY, iplGrayMat, grayOffscreen);

		res = ASFProcessEx_IR(m_hEngine, &grayOffscreen, &detectedFaces, ASF_IR_LIVENESS);

		res = ASFGetLivenessScore_IR(m_hEngine, &livenessInfo);
	}

	cvReleaseImage(&cutImg);
	return res;
}

void PicCutOut(IplImage* src, IplImage* dst, int x, int y)
{
	if (!src || !dst)
	{
		return;
	}

	CvSize size = cvSize(dst->width, dst->height);
	cvSetImageROI(src, cvRect(x, y, size.width, size.height));
	cvCopy(src, dst);
	cvResetImageROI(src);
	src = dst;
}

int ColorSpaceConversion(MInt32 format, IplImage* img, ASVLOFFSCREEN& offscreen)
{
	offscreen.u32PixelArrayFormat = (unsigned int)format;
	offscreen.i32Width = img->width;
	offscreen.i32Height = img->height;

	switch (format)		//原始图像颜色格式
	{
	case ASVL_PAF_RGB24_B8G8R8:
	case ASVL_PAF_GRAY:
	case ASVL_PAF_DEPTH_U16:
	case ASVL_PAF_YUYV:
		offscreen.pi32Pitch[0] = img->widthStep;
		offscreen.ppu8Plane[0] = (MUInt8*)img->imageData;
		break;
	case ASVL_PAF_NV21:
	case ASVL_PAF_NV12:
		offscreen.pi32Pitch[0] = img->widthStep;
		offscreen.pi32Pitch[1] = offscreen.pi32Pitch[0];
		offscreen.ppu8Plane[0] = (MUInt8*)img->imageData;
		offscreen.ppu8Plane[1] = offscreen.ppu8Plane[0] + offscreen.pi32Pitch[0] * offscreen.i32Height;
		break;
	case ASVL_PAF_I420:
		offscreen.pi32Pitch[0] = img->widthStep;
		offscreen.pi32Pitch[1] = offscreen.pi32Pitch[0] >> 1;
		offscreen.pi32Pitch[2] = offscreen.pi32Pitch[0] >> 1;
		offscreen.ppu8Plane[0] = (MUInt8*)img->imageData;
		offscreen.ppu8Plane[1] = offscreen.ppu8Plane[0] + offscreen.i32Height * offscreen.pi32Pitch[0];
		offscreen.ppu8Plane[2] = offscreen.ppu8Plane[0] + offscreen.i32Height * offscreen.pi32Pitch[0] * 5 / 4;
		break;
	default:
		return 0;
	}
	return 1;
}
