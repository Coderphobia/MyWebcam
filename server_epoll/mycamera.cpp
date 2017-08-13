
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include "mycamera.h"
#include "jpeg_enc.h"

#define FMT  V4L2_PIX_FMT_YUYV  /* 摄像头采集的图片数据格式为yuyv */

using namespace std;

MyCamera::MyCamera(string dev_file, int width, int height)
{
	inited = false;

	w = width;
	h = height;

	buf_yuyv = new unsigned char[w*h*2];

	fd = open(dev_file.data(), O_RDWR);
	if (fd < 0)
	{
		perror("open dev");
		return;
	}

	struct v4l2_capability cap;
	if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0)
		return;
	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
	{
		printf("unsupported camera\n");
		return;
	}

   /* 1、设置格式，分辨率 */
	struct v4l2_format  format;
	memset(&format, 0, sizeof(format));
	
	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	format.fmt.pix.width = w;
	format.fmt.pix.height = h;
	format.fmt.pix.pixelformat = FMT; 

	if (ioctl(fd, VIDIOC_S_FMT, &format) < 0)
	{
		perror("set format");
		return;
	}
	
	ioctl(fd, VIDIOC_G_FMT, &format);
	if (format.fmt.pix.pixelformat != FMT)
	{
		printf("unsupport format\n");
		return;
	}

   /* 2、让驱动申请出存放图像的缓冲区 */
	struct v4l2_requestbuffers bufs;
	bufs.count = BUF_COUNT;
	bufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	bufs.memory = V4L2_MEMORY_MMAP; /* 申请出来的缓冲区用于mmap */
	
	if (ioctl(fd, VIDIOC_REQBUFS, &bufs) < 0)
	{
		perror("reqbufs");
		return;
	}

   /* 3、查询驱动里申请出来的缓冲区状态 */
	struct v4l2_buffer buffer;
	memset(&buffer, 0, sizeof(buffer));

	buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buffer.memory = V4L2_MEMORY_MMAP;
	for (int i = 0; i < BUF_COUNT; i++)
	{
		buffer.index = i;
		if (ioctl(fd, VIDIOC_QUERYBUF, &buffer) < 0)
		{
			perror("querybuf");
			return;
		}
		printf("buffer%d: length = %d, offset = %d\n", \
                      i, buffer.length, buffer.m.offset);

		/* 4、把驱动里的缓冲区映射到用户进程 */
		pic_buf[i] = (unsigned char *)mmap(NULL, buffer.length, \
                      PROT_READ, MAP_SHARED, fd, buffer.m.offset);
		if (MAP_FAILED == pic_buf[i])
		{
			perror("mmap");
			return;
		}

		/* 5、把缓冲区加入图像的采集队列。只有在采集队列里，
		 * 驱动里才会给这些缓冲区填入摄像头的数据 */
		if (ioctl(fd, VIDIOC_QBUF, &buffer) < 0)
		{
			perror("qbuf");
			return;
		}	
	}

   /* 6、启动采集 */
	int on = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(fd, VIDIOC_STREAMON, &on) < 0)
	{
		perror("stream on");
		return;
	}

	inited = true;
}

MyCamera::~MyCamera()
{
	delete buf_yuyv;

	int off = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ioctl(fd, VIDIOC_STREAMOFF, &off);

	for (int i = 0; i < BUF_COUNT; i++)
		munmap(pic_buf[i], w*h*2);	 
	close(fd);
}

bool MyCamera::isInited() const
{
	return inited;
}

int MyCamera::getFd() const
{
	return fd;
}


/* 缓冲区出队， 获取yuyv数据 */
int MyCamera::getYuyv(unsigned char *data, int data_len) const
{
	struct v4l2_buffer buf;
	memset(&buf, 0, sizeof(buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	
	if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0)
	{
		perror("dqbuf");
		return -1;
	}	

	int len = buf.bytesused;
	if (data_len < len)	
	{
		ioctl(fd, VIDIOC_QBUF, &buf);	
		return -2;	
	}

	memcpy(data, pic_buf[buf.index], len);
	ioctl(fd, VIDIOC_QBUF, &buf);

	return len;
}

/* TODO: */
int MyCamera::getYuyv420p(unsigned char *data, int data_len) const
{

	return 0;
}

#define clamp(v)  (((v<0)?0:v)>255?255:v)
int MyCamera::getPnm(unsigned char *data, int data_len) const
{
	int ret = getYuyv(buf_yuyv, w*h*2);
	if (ret < 0)
		return ret;

	int x, y;
	int y0, u0, y1, v1;
	int n;
	unsigned char *rgb;

	sprintf((char *)data, "P6\n%d %d\n255\n", w, h);
	rgb = data+strlen((char *)data);

	for (y = 0; y < h; y++)
	{
		for (x = 0; x < w; x += 2)
		{
			n = y*w+x; /* 第几像素 */
			y0 = buf_yuyv[n*2+0];
			u0 = buf_yuyv[n*2+1];
			y1 = buf_yuyv[n*2+2];
			v1 = buf_yuyv[n*2+3];
			/* y0u0v1 */
			rgb[n*3+0] = clamp(y0 + 1.402 * (v1-128));
			rgb[n*3+1] = clamp(y0 - 0.344 * (u0-128) - 0.714 * (v1-128));
			rgb[n*3+2] = clamp(y0 + 1.772 * (u0-128));
			/* y1u0v1 */
			rgb[n*3+3] = clamp(y1 + 1.402 * (v1-128));
			rgb[n*3+4] = clamp(y1 - 0.344 * (u0-128) - 0.714 * (v1-128));
			rgb[n*3+5] = clamp(y1 + 1.772 * (u0-128));
		}
	}
	return strlen((char *)data) + w*h*3;
}

int MyCamera::getJpg(unsigned char *data, int data_len) const
{
	int ret = getYuyv(buf_yuyv, w*h*2);
	if (ret < 0)
		return ret;

	return encode_image(buf_yuyv, data, 200, w, h);	
}

