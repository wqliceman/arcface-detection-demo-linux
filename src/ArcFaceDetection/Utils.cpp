#include "Utils.h"
//#include <strmif.h>
//#include <initguid.h>
#include <QImage>
#include <QPixmap>
#include <QSettings>
#include <QStringList>
#include <QFileDialog>
#include <QDirIterator>
#include <QDebug>
#include "config.h"

#include <opencv2/videoio.hpp>

//#pragma comment(lib, "setupapi.lib")

#define VI_MAX_CAMERAS 20
//DEFINE_GUID(CLSID_SystemDeviceEnum, 0x62be5d10, 0x60eb, 0x11d0, 0xbd, 0x3b, 0x00, 0xa0, 0xc9, 0x11, 0xce, 0x86);
//DEFINE_GUID(CLSID_VideoInputDeviceCategory, 0x860bb310, 0x5d01, 0x11d0, 0xbd, 0x3b, 0x00, 0xa0, 0xc9, 0x11, 0xce, 0x86);
//DEFINE_GUID(IID_ICreateDevEnum, 0x29840822, 0x5b84, 0x11d0, 0xbd, 0x3b, 0x00, 0xa0, 0xc9, 0x11, 0xce, 0x86);

Utils::Utils()
{ }


Utils::~Utils()
{ }

//列出摄像头
void Utils::listDevices(vector<int>& list)
{
    for (auto i = 0; i != 10; ++i) {
        auto cap = cv::VideoCapture(i);
        if (cap.isOpened()) { list.emplace_back(i); }
        cap.release();
    }
    /*
	ICreateDevEnum *pDevEnum = NULL;
	IEnumMoniker *pEnum = NULL;
	int deviceCounter = 0;
	CoInitialize(NULL);

	HRESULT hr = CoCreateInstance(
		CLSID_SystemDeviceEnum,
		NULL,
		CLSCTX_INPROC_SERVER,
		IID_ICreateDevEnum,
		reinterpret_cast<void**>(&pDevEnum)
	);

	if (SUCCEEDED(hr))
	{
		hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0);
		if (hr == S_OK) {

			IMoniker *pMoniker = NULL;
			while (pEnum->Next(1, &pMoniker, NULL) == S_OK)
			{
				IPropertyBag *pPropBag;
				hr = pMoniker->BindToStorage(0, 0, IID_IPropertyBag,
					(void**)(&pPropBag));

				if (FAILED(hr)) {
					pMoniker->Release();
					continue; // Skip this one, maybe the next one will work.
				}

				VARIANT varName;
				VariantInit(&varName);
				hr = pPropBag->Read(L"Description", &varName, 0);
				if (FAILED(hr))
				{
					hr = pPropBag->Read(L"FriendlyName", &varName, 0);
				}

				if (SUCCEEDED(hr))
				{
					hr = pPropBag->Read(L"FriendlyName", &varName, 0);
					int count = 0;
					char tmp[255] = { 0 };
					while (varName.bstrVal[count] != 0x00 && count < 255)
					{
						tmp[count] = (char)varName.bstrVal[count];
						count++;
					}
					list.push_back(tmp);
				}

				pPropBag->Release();
				pPropBag = NULL;
				pMoniker->Release();
				pMoniker = NULL;

				deviceCounter++;
			}

			pDevEnum->Release();
			pDevEnum = NULL;
			pEnum->Release();
			pEnum = NULL;
		}
	}
        return deviceCounter;
        */
}


QImage Utils::cvMatToQImage(const cv::Mat& mat)
{
	// 8-bits unsigned, NO. OF CHANNELS = 1  
	if (mat.type() == CV_8UC1) {
		QImage image(mat.cols, mat.rows, QImage::Format_Indexed8);
		// Set the color table (used to translate colour indexes to qRgb values)  
		image.setColorCount(256);
		for (int i = 0; i < 256; i++)
			image.setColor(i, qRgb(i, i, i));

		// Copy input Mat  
		uchar *pSrc = mat.data;
		for (int row = 0; row < mat.rows; row++) {
			uchar *pDest = image.scanLine(row);
			memcpy(pDest, pSrc, mat.cols);
			pSrc += mat.step;
		}
		return image;
	}
	// 8-bits unsigned, NO. OF CHANNELS = 3  
	else if (mat.type() == CV_8UC3) {
		// Copy input Mat  
		const uchar *pSrc = (const uchar*)mat.data;
		// Create QImage with same dimensions as input Mat  
		QImage image(pSrc, mat.cols, mat.rows, mat.step, QImage::Format_RGB888);
		return image.rgbSwapped();
	}
	else if (mat.type() == CV_8UC4) {
		//qDebug() << "CV_8UC4";
		// Copy input Mat  
		const uchar *pSrc = (const uchar*)mat.data;
		// Create QImage with same dimensions as input Mat  
		QImage image(pSrc, mat.cols, mat.rows, mat.step, QImage::Format_ARGB32);
		return image.copy();
	}
	else {
		//qDebug() << "ERROR: Mat could not be converted to QImage.";
		return QImage();
	}
}

#define strcpy_s(dst, n, src) strncpy(dst, src, n)

void Utils::readSetting(
    char appId[], char sdkKey[],
    float& rgbLiveThreshold, float& irLiveThreshold, float& rgbFqThreshold,
    int& rgbCameraId, int& irCameraId,
    int& compareModel
)
{
    QSettings settings(SETTING_FILE, QSettings::IniFormat);
    qDebug() << QDir::currentPath();

    settings.beginGroup("ActiveInfo");
	QString strAppId = settings.value("appId").toString();
    strcpy_s(appId, 64, strAppId.toStdString().c_str());
	QString strSdkKey = settings.value("sdkKey").toString();
	strcpy_s(sdkKey, 64, strSdkKey.toStdString().c_str());
	settings.endGroup();

	settings.beginGroup("Setting");
	rgbLiveThreshold = settings.value("rgbLiveThreshold").toFloat();
	irLiveThreshold = settings.value("irLiveThreshold").toFloat();
	rgbFqThreshold = settings.value("rgbFqThreshold").toFloat();
	rgbCameraId = settings.value("rgbCameraId").toInt();
	irCameraId = settings.value("irCameraId").toInt();
	compareModel = settings.value("compareModel").toInt();
	settings.endGroup();
}

QStringList Utils::getDestFolderImages()
{
	QStringList imageList;
	QFileInfo fileInfo;
	QString filePath;
	QString suffixName;

	//选择文件夹路径
    //parent->hide();
    QString folderPath = QFileDialog::getExistingDirectory(
        nullptr, "choose src Directory", "/home",
        QFileDialog::DontUseNativeDialog
    );
    //parent->show();
    if (folderPath.isEmpty())
	{
		return imageList;
	}

	QDir dir(folderPath);
	//仅获取指定路径下的所有文件，不递归查找
	QFileInfoList allFilePath = dir.entryInfoList();
	for (int i = 0; i < allFilePath.size(); ++i)
	{
		fileInfo = allFilePath.at(i);
		suffixName = fileInfo.suffix();
		if (suffixName == "jpg" || suffixName == "JPG" || suffixName == "png" ||
			suffixName == "PNG" || suffixName == "bmp" || suffixName == "BMP")
		{
			filePath = fileInfo.filePath();
			imageList << filePath;
		}
	}
	return imageList;
}

QStringList Utils::getFolderAllImages()
{
	QStringList imageList;
	QFileInfo fileinfo;
	QString filePath;
	QString suffixName;

	//选择文件夹路径
	QString folderPath = QFileDialog::getExistingDirectory(nullptr, "choose src Directory", NULL);
	if (folderPath.isEmpty())
	{
		return imageList;
	}

	//遍历文件夹下所有文件
	QDirIterator allFilePath(folderPath, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
	while (allFilePath.hasNext())
	{
		filePath = allFilePath.next();
		fileinfo = QFileInfo(filePath);
		suffixName = fileinfo.suffix();
		if (suffixName == "jpg" || suffixName == "JPG" || suffixName == "png" ||
			suffixName == "PNG" || suffixName == "bmp" || suffixName == "BMP")
		{
			imageList << filePath;
		}
	}
	return imageList;
}

//void Utils::cvText(IplImage* img, const char* text, int x, int y)
//{
//	CvFont font;
//	double hscale = 2;
//	double vscale = 2;
//	int linewidth = 3;
//	cvInitFont(&font, CV_FONT_HERSHEY_SIMPLEX | CV_FONT_ITALIC, hscale, vscale, 0, linewidth);
//	CvScalar textColor = cvScalar(0, 255, 255);
//	CvPoint textPos = cvPoint(x, y);
//	cvPutText(img, text, textPos, &font, textColor);
//}
/*
std::string Utils::UTF8_To_string(const std::string & str)
{
	int nwLen = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);

	wchar_t * pwBuf = new wchar_t[nwLen + 1];//一定要加1，不然会出现尾巴 
    memset(pwBuf, 0, (nwLen+1) * sizeof(wchar_t));

	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.length(), pwBuf, nwLen);

	int nLen = WideCharToMultiByte(CP_ACP, 0, pwBuf, -1, NULL, NULL, NULL, NULL);

	char * pBuf = new char[nLen + 1];
	memset(pBuf, 0, nLen + 1);

	WideCharToMultiByte(CP_ACP, 0, pwBuf, nwLen, pBuf, nLen, NULL, NULL);

	std::string retStr = pBuf;

	delete[]pBuf;
	delete[]pwBuf;

	pBuf = NULL;
	pwBuf = NULL;

	return retStr;
}

std::string Utils::string_To_UTF8(const std::string & str)
{
	int nwLen = ::MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, NULL, 0);

	wchar_t * pwBuf = new wchar_t[nwLen + 1];//一定要加1，不然会出现尾巴 
	ZeroMemory(pwBuf, nwLen * 2 + 2);

	::MultiByteToWideChar(CP_ACP, 0, str.c_str(), str.length(), pwBuf, nwLen);

	int nLen = ::WideCharToMultiByte(CP_UTF8, 0, pwBuf, -1, NULL, NULL, NULL, NULL);

	char * pBuf = new char[nLen + 1];
	ZeroMemory(pBuf, nLen + 1);

	::WideCharToMultiByte(CP_UTF8, 0, pwBuf, nwLen, pBuf, nLen, NULL, NULL);

	std::string retStr(pBuf);

	delete[]pwBuf;
	delete[]pBuf;

	pwBuf = NULL;
	pBuf = NULL;

	return retStr;
}
*/
