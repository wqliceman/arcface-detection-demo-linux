#pragma once
#include "arcsoft_face_sdk.h"
#include "merror.h"
#include "opencv/cv.h"
#include "opencv/highgui.h"


class FaceEngine
{
public:
	FaceEngine();
	~FaceEngine();

public:
	void initData();

	//激活SDK
	MRESULT ActiveSDK(char* appId, char* sdkKey, char* activeKey);
	//获取激活信息
	MRESULT GetActiveFileInfo(ASF_ActiveFileInfo& activeFileInfo);
	//引擎初始化
	MRESULT InitEngine(ASF_DetectMode detectMode, ASF_OrientPriority detectFaceOrientPriority);
	//释放引擎
	MRESULT UnInitEngine();
	//人脸检测
	MRESULT PreDetectFace(IplImage* image, ASF_MultiFaceInfo& multiFaceInfo, bool isRgb);
	//图像质量检测
	MRESULT PreImageQualityDetect(IplImage* image, ASF_MultiFaceInfo multiFaceInfo, float rgbFqThreshold);
	//人脸特征提取
	MRESULT PreExtractFeature(IplImage* image, ASF_SingleFaceInfo& faceRect, 
        ASF_FaceFeature& feature);
	//人脸比对
	MRESULT FacePairMatching(MFloat &confidenceLevel, ASF_FaceFeature feature1, ASF_FaceFeature feature2, 
		ASF_CompareModel compareModel = ASF_LIFE_PHOTO);
	//设置活体阈值
	MRESULT SetLivenessThreshold(MFloat	rgbLiveThreshold, MFloat irLiveThreshold);
	//人脸属性检测（年龄、性别、活体、3D角度）
	MRESULT FaceProcess(ASF_MultiFaceInfo detectedFaces, IplImage *img, ASF_AgeInfo &ageInfo,
		ASF_GenderInfo &genderInfo, ASF_Face3DAngle &angleInfo, ASF_LivenessInfo& liveNessInfo);
	//活体检测
	MRESULT livenessProcess(ASF_MultiFaceInfo detectedFaces, IplImage *img, ASF_LivenessInfo& livenessInfo, bool isRgb);

private:
	MHandle m_hEngine;
};

//图像裁剪
void PicCutOut(IplImage* src, IplImage* dst, int x, int y);
// 图像颜色空间转换
int ColorSpaceConversion(MInt32 format, IplImage* img, ASVLOFFSCREEN& offscreen);
