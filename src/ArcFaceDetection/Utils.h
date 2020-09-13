#pragma once
#include <vector>
#include <string>
#include "opencv/cv.h"
#include "opencv/highgui.h"

#include <QWidget>

using namespace std;

class QImage;
class QPixmap;
class QStringList;

class Utils
{
public:
	Utils();
	~Utils();


public:
    void listDevices(vector<int>& list);
	QImage cvMatToQImage(const cv::Mat& mat);
	void readSetting(char appId[], char sdkKey[], float& rgbLiveThreshold, float& irLiveThreshold,
        float& rgbFqThreshold, int& rgbCameraId, int& irCameraId, int& compareModel);
    QStringList getDestFolderImages();		//当前文件夹不递归查找
	QStringList getFolderAllImages();		//当前文件夹递归查找

	std::string UTF8_To_string(const std::string & str);
	std::string string_To_UTF8(const std::string & str);

};

