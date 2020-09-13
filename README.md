# arcface-detection-demo-linux
ArcSoft_ArcFace_Linux_x64_V3.0 demo

基于Ubuntu 64下Qt编写demo，该版本基于虹软离线免费版本开发。修改于 [**arface-demo-linux**](https://github.com/tz-byte/arcface-demo-linux)



### ArcFace SDK

到虹软官网下载[人脸识别 SDK 3.0 Linux 版本](https://ai.arcsoft.com.cn/product/arcface.html) 解压到合适的目录，并从官网获取 APP_ID、SDK_KEY ，用于写到配置文件用来激活 SDK。

### OpenCV

 到 OpenCV 官网下载[源码](https://opencv.org/releases/)，我用的版本是 [3.4.9](https://github.com/opencv/opencv/archive/3.4.9.zip)。可以按照官网的教程 [Installation in Linux](https://docs.opencv.org/3.4.9/d7/d9f/tutorial_linux_install.html) 自行编译，我参考官网教程使用下面的这些命令在 GCC 9.3.0（Ubuntu 20.04 自带的编译器） 上编译成功。

``` bash
sudo apt update
sudo apt install build-essential
sudo apt install cmake git libgtk2.0-dev pkg-config libavcodec-dev libavformat-dev libswscale-dev
cd <OpenCV 源码目录>
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=<自定义目录> ..
make -j3    # 可以使用核心数 - 1 个线程来编译
sudo make install
```

### Qt 

Qt 使用的是 [5.14.2](http://download.qt.io/archive/qt/5.14/5.14.2/qt-opensource-linux-x64-5.14.2.run) 版本。

### 配置文件

Demo 可以只打开一个RGB摄像头，也可以同时打开一个RGB摄像头和一个IR摄像头。原代码保存获取摄像头的名称，仅用来统计数量，具体打开哪个摄像头是通过`settings.ini`文件来配置的。在改变探测摄像头存在的数量的方式后，顺带改变了打开摄像头的逻辑，仅一个摄像头就认为是仅打开普通摄像头。在`settings.ini`文件中配置两种摄像头的索引，如果索引为 -1，则自动把小的索引认为是普通摄像头，大的索引认为是红外摄像头，如果和真实情况不一致可手动指定摄像头索引。

> `settings.ini`文件在后面运行 Demo 时会有更多的说明。



1. 配置文件已经随源码打包好了，在运行时需要移动到可执行程序所在的同级目录下。
2. 在配置文件中填入官网获取的 APP_ID、SDK_KEY。
3. 编译并运行。