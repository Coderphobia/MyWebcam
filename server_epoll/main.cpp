
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <iostream>
#include <list>
#include "mycamera.h"

#define WIDTH       640
#define HEIGHT      480
#define PORT        5566
#define LEN         (WIDTH*HEIGHT*2)
#define P_LEN       1300
#define VIDEO_DEV   "/dev/video0"

/* 服务器端为每个与之通信的客户端创建该类的对象，
 * 分别记录每个客户端图像的收发情况等相关信息 */
class client_data_t {
public:
	client_data_t()
	{
		memset(this, 0, sizeof(client_data_t));
		status = -1; /* 空闲状态 */
	}
	int fd;
	unsigned char jpg_data[LEN]; /* 发给客户端的jpg图像 */
	int jpg_len;  /* jpg长度 */
	int sent_len; /* 已发送的长度 */
	int status;   /* -1表示空闲, 0表示客户已请求新图像, 
                     1表示正在发送中还不可更新图像数据 */
};

using namespace std;

int init_tcpserver(int port);

int main(void)
{
	int ret;
	int sock_fd; /* tcp服务器，socket文件描符述 */
	int fd_cam;  /* 摄像头文件描述符 */
	list<client_data_t *> client_list; /* 用连表管理所有客户端信息 */
	unsigned char *data = new unsigned char[LEN]; /* 存放图像数据 */
	MyCamera *cam = new MyCamera(VIDEO_DEV, WIDTH, HEIGHT);

	if (cam->isInited())
		fd_cam = cam->getFd();
	else
		return -1;

	sock_fd = init_tcpserver(PORT);
	if (sock_fd < 0)
	{
		printf("tcp server init failed\n");
		return -1;
	}

	int epfd = epoll_create1(0);
	struct epoll_event evt, revts[10];
	int num, i, fd;
	char cmd[200];
	client_data_t *mydata = new client_data_t;
	
	/* 监控摄像头文件描述符的EPOOLIN事件，
	 * 当有数据可读时，epoll_wait会返回该事件 */
	mydata->fd = fd_cam;
	evt.events = EPOLLIN;
	evt.data.ptr = mydata;
	epoll_ctl(epfd, EPOLL_CTL_ADD, fd_cam, &evt);

	/* 监控服务器文件描述符的EPOLLIN事件，
	 * 当新客户端请求连接、客户端断开连接时,
	 * epoll_wait会返回该事件 */
	mydata = new client_data_t;
	mydata->fd = sock_fd;
	evt.data.ptr = mydata;
	epoll_ctl(epfd, EPOLL_CTL_ADD, sock_fd, &evt);
 
	while (1)
	{
		num = epoll_wait(epfd, revts, 10, -1);
		if (num <= 0)
			continue;
		for (i = 0; i < num ; i++)
		{
			mydata = (client_data_t *)revts[i].data.ptr;
			fd = mydata->fd;
			/* 1、有新采集好的图像 */ 
			if (fd == fd_cam) 
			{
				ret = cam->getJpg(data, LEN);
				for (auto it = client_list.begin(); it != client_list.end(); it++)
				{
					mydata = (*it);
					if (0 == mydata->status) /* 给已请求图像的客户端更换数据 */
					{
						mydata->status = 1;
						memcpy(mydata->jpg_data, data, ret);
						mydata->jpg_len = ret;
						/*给客户端发出第一部分数据 */
						mydata->sent_len = write(mydata->fd, data, P_LEN); 
						printf("jpg_len = %d\n", mydata->jpg_len);
					}
				}
				continue;
			}

			/* 2、有新的客户端请求连接 */
			if (fd == sock_fd)
			{
				fd = accept(sock_fd, NULL, NULL);
				mydata = new client_data_t;
				mydata->fd = fd;
				evt.data.ptr = mydata;
				epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &evt);  /* 加入epoll监听 */
				printf("new connection ...%d\n", fd);
				client_list.push_back(mydata); /* 加入客户端链 */
				continue;
			}

			/* 3、来自客户端的消息 */
			ret = read(fd, cmd, sizeof(cmd));
			if (ret <= 0) /* 客户端离线 */
			{
				client_list.remove(mydata);
				delete mydata;
				epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
				close(fd);
				continue;
			}		
			cmd[ret] = 0;			
			printf("from %d : %s\n", fd, cmd);
			
			/* 收到客户端的"REQUEST"，请求新图像数据 */
			if (0 == strcmp("REQUEST", cmd))
			{
				mydata->status = 0; /* 转为已请求新图像状态 */
				continue;
			}
			if (0 != strcmp("ACK", cmd))
				continue;

			/* 收到客户端的"ACK"，继续发图像剩余的部分 */
			if ((mydata->sent_len + P_LEN) > mydata->jpg_len)
				ret = write(fd, mydata->jpg_data+mydata->sent_len, 
					mydata->jpg_len - mydata->sent_len);
			else
				ret = write(fd, mydata->jpg_data+mydata->sent_len, P_LEN);
			mydata->sent_len += ret;

			if (mydata->sent_len >= mydata->jpg_len)
				mydata->status = -1; /* 发送完毕，该客户端转为空闲状态 */
		}	
	}
	return 0;
}

/*
 * 启动服务器，@port为服务器的端口号
 */
int init_tcpserver(int port)
{
	int sock_fd;

	sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (sock_fd < 0)
		return -1;

	int val = 1;
	setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = 0;

	if (bind(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
	{
		perror("bind");
		return -1;
	}
	
	if (listen(sock_fd, 100) < 0)
	{
		perror("listen");
		return -1;	
	}

	return sock_fd;
}

