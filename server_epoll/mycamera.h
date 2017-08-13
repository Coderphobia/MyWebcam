
#ifndef _MYCAMERA_H_
#define _MYCAMERA_H_

#include <iostream>

#define BUF_COUNT  3  

using namespace std;

class MyCamera {
private:
	int fd;  /* 摄像头对应的文件描述符 */
	unsigned char *pic_buf[BUF_COUNT];  /* 存放yuyv数据的驱动缓冲区 */
	bool inited;  /* 摄像头是否初始化成功 */
	int w, h;  /* 图片的宽和高 */
	unsigned char *buf_yuyv; /* 缓冲区yuyv数据出队存放 */

public:
	MyCamera(string dev_file = "/dev/video0", int width = 640, int height = 480);
	~MyCamera();

	bool isInited() const;
	int getFd() const;
	int getYuyv(unsigned char *data, int data_len) const;
	int getYuyv420p(unsigned char *data, int data_len) const;
	int getPnm(unsigned char *data, int data_len) const;
	int getJpg(unsigned char *jpg, int data_len) const;
};

#endif /* _MYCAMERA_H_ */

