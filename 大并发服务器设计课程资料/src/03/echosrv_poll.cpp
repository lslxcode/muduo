/** 
POLLIN事件
	内核中的socket接收缓存区为空     低电平
	内核中的socket接收缓存区不为空   高电平


POLLOUT事件
	内核中的socket发送缓存区不满     高电平
	内核中的socket发送缓存区满了     低电平

LT 电平触发
	高电平持续触发

ET 边沿触发

低电平---》高电平  触发
高电平---》低电平  触发
*/



/**
LT(电平触发模式的通用做法，poll不支持水平触发（ET）模式)
	1. poll中不应该用阻塞套接字，不然会把整个进程阻塞掉，影响别的套接字的事件处理
	2. 非阻塞状态下，如果写缓存区和接收缓存区满了，就需要有一个应用层的发送缓存区和接收缓存区
		int ret = write(fd,buf,10000);
		if(ret<0){
			// 内核缓存区满了，没有全部发送，需要注册fd的POLLOUT事件
		}
		
	3. 如果没有发送完毕,就要注册POLLOUT事件，当内核中的缓存区有空间的时候，就触发POLLOUT事件
	4. 如果应用层缓存区已经把所以数据发发送完了，就需要取消注册POLLOUT事件，不然会出现BUSY LOOP（忙等待）
	5. 不能在accept注册POLLOUT事件，只能在要发送数据的时候注册POLLOUT事件，不然会出现BUSY LOOP（忙等待）
	
	
ccept(2)返回EMFILE的处理（文件描述符数目超出限制），如果没有及时处理，可能会发生BUSY LOOP（忙等待）
1. 调高进程文件描述符数目（不是最优，系统资源总有耗尽的情况）
2. 死等（不是最优）
3. 退出程序（不是最优）
4. 关闭监听套接字（那什么时候重新打开呢？）
5. 如果是epoll模型，可以改为边沿触发模型（ET）模式（如果漏掉了一次accept，程序就再也接收不到新的连接）
6. 准备一个新的空闲的文件描述符
	先关闭这个空闲文件，获得一个文件描述符名额，再accept拿到socket连接的文件描述符，随后立即close，这样
	就优雅的断开了与客户端的连接，最后重新打开这个空闲文件，把“坑”填上，以备后续使用

*/


/**
（ET）模式
	1. accept之后，就注册POLLOUT事件，不会出现BUSY LOOP（忙等待），不可发送数据到可发送数据只触发一次
	2. EOPLLIN事件，从低电平到高电平（无--->有），数据可读（只触发一次），一定要把所有的数据都读完，如果接收缓存区空了还继续读，read函数返回就会产生EAGAIN错误，如果没有全部read，就可能再也没read了
		直到发生EAGAIN错误为止才能读完
	3. 内核缓存区如果满了，就代表低电平状态，若内核缓存区空出来位置，就会变成高电平，产生EPOLLOUT事件，应用层就会发送数据，直到数据发送完，或产生EAGAIN错误，如果没有全部发送完毕，就可能再也没机会发了
	5. accept(2)返回EMFILE失败，如果服务端未处理，就不会再触发accept事件了，不会发生BUSY LOOP（忙等待），但是这个待accept套接字就再也没机会accept了
*/


#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/wait.h>
#include <poll.h>

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <vector>
#include <iostream>

#define ERR_EXIT(m) \
       do \
        { \
                perror(m); \
                exit(EXIT_FAILURE); \
        } while(0)

/// 动态数组
typedef std::vector<struct pollfd> PollFdList;

int main(void)
{
	/// 如果客户端关闭了套接字close,服务器调用了一次write，服务器就会收到一个RST（TCP传输层）
	/// 如果服务端再次调用了write，服务端就会收到SIGPIPE信号，默认处理方式是进程退出，所以我们要忽略这个信号
	signal(SIGPIPE, SIG_IGN);
	
	/// 如果服务端主动调用close（先于客户端调用close），服务器就会主动进入TIME_WAIT状态
	/// 协议设计上，用该让客户端主动断开连接，这样就把TIME_WAIT分散到大量的客户端上面去了
	/// 如果客户端一直不断开与服务器的请求，就会一直占用服务端的资源，所以服务器应该有一个机制主动断开不活跃的请求
	signal(SIGCHLD, SIG_IGN);

	/// 处理EMFILE的问题的暂时的解决方案
	//int idlefd = open("/dev/null", O_RDONLY | O_CLOEXEC);
	int listenfd;

	/// F_GETFD获取文件描述符的状态，F_SETFD设置文件描述符的状态
	//if ((listenfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
	/// SOCK_NONBLOCK 非阻塞套接字
	/// SOCK_CLOEXEC 如果folk一个子进程时，子进程默认的状态时关闭状态
	if ((listenfd = socket(PF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP)) < 0)
		ERR_EXIT("socket");

	struct sockaddr_in servaddr;
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(5188);
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

	int on = 1;
	/// 设置地址的重复利用
	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
		ERR_EXIT("setsockopt");

	/// 绑定文件描述符到地址
	if (bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
		ERR_EXIT("bind");
	
	/// 监听
	if (listen(listenfd, SOMAXCONN) < 0)
		ERR_EXIT("listen");

	struct pollfd pfd;
	pfd.fd = listenfd;
	
	/// POLLIN 有数据可读
	pfd.events = POLLIN;

	/// 将文件描述符添加到动态数组
	PollFdList pollfds;
	pollfds.push_back(pfd);

	int nready;

	struct sockaddr_in peeraddr;
	socklen_t peerlen;
	int connfd;

	while (1)
	{
		/// 在C++11中 &*pollfds.begin() == pollfds.data()
		/// -1永远等待
		nready = poll(&*pollfds.begin(), pollfds.size(), -1);
		if (nready == -1)
		{
			if (errno == EINTR)
				continue;
			
			ERR_EXIT("poll");
		}
		if (nready == 0)	// nothing happended
			continue;
		
		if (pollfds[0].revents & POLLIN)
		{
			peerlen = sizeof(peeraddr);
			/// 如果使用accept没有设置连接文件描述符的功能，accept4支持
			/// 设置连接文件描述符 SOCK_NONBLOCK | SOCK_CLOEXEC 非阻塞套接字，子进程默认的状态时关闭状态
			connfd = accept4(listenfd, (struct sockaddr*)&peeraddr,
						&peerlen, SOCK_NONBLOCK | SOCK_CLOEXEC);

			if (connfd == -1)
				ERR_EXIT("accept4");

/*
		/// accept(2)返回EMFILE的处理
		/// 暂时的与客户端优雅的断开错误
			if (connfd == -1)
			{
				if (errno == EMFILE)
				{
					close(idlefd);
					idlefd = accept(listenfd, NULL, NULL);
					close(idlefd);
					idlefd = open("/dev/null", O_RDONLY | O_CLOEXEC);
					continue;
				}
				else
					ERR_EXIT("accept4");
			}
*/

			/// 将已连接的套接字加入到监听的动态数组中
			pfd.fd = connfd;
			pfd.events = POLLIN;
			pfd.revents = 0;
			pollfds.push_back(pfd);
			--nready;

			// 连接成功
			std::cout<<"ip="<<inet_ntoa(peeraddr.sin_addr)<<
				" port="<<ntohs(peeraddr.sin_port)<<std::endl;
			if (nready == 0)
				continue;
		}

		//std::cout<<pollfds.size()<<std::endl;
		//std::cout<<nready<<std::endl;
		/// 轮询遍历已连接套接字发生的事件
		for (PollFdList::iterator it=pollfds.begin()+1;
			it != pollfds.end() && nready >0; ++it)
		{
				if (it->revents & POLLIN)
				{
					--nready;
					connfd = it->fd;
					char buf[1024] = {0};
					int ret = read(connfd, buf, 1024);
					if (ret == -1)
						ERR_EXIT("read");
					if (ret == 0)
					{
						std::cout<<"client close"<<std::endl;
						it = pollfds.erase(it);
						/// 在C++vector迭代器中，当使用erase时，迭代器默认会指向下一个元素所在的地址，所以这里就--操作
						--it;

						close(connfd);
						continue;
					}

					std::cout<<buf;
					write(connfd, buf, strlen(buf));
					
				}
		}
	}

	return 0;
}


