/** 
POLLIN�¼�
	�ں��е�socket���ջ�����Ϊ��     �͵�ƽ
	�ں��е�socket���ջ�������Ϊ��   �ߵ�ƽ


POLLOUT�¼�
	�ں��е�socket���ͻ���������     �ߵ�ƽ
	�ں��е�socket���ͻ���������     �͵�ƽ

LT ��ƽ����
	�ߵ�ƽ��������

ET ���ش���

�͵�ƽ---���ߵ�ƽ  ����
�ߵ�ƽ---���͵�ƽ  ����
*/



/**
LT(��ƽ����ģʽ��ͨ��������poll��֧��ˮƽ������ET��ģʽ)
	1. poll�в�Ӧ���������׽��֣���Ȼ�������������������Ӱ�����׽��ֵ��¼�����
	2. ������״̬�£����д�������ͽ��ջ��������ˣ�����Ҫ��һ��Ӧ�ò�ķ��ͻ������ͽ��ջ�����
		int ret = write(fd,buf,10000);
		if(ret<0){
			// �ں˻��������ˣ�û��ȫ�����ͣ���Ҫע��fd��POLLOUT�¼�
		}
		
	3. ���û�з������,��Ҫע��POLLOUT�¼������ں��еĻ������пռ��ʱ�򣬾ʹ���POLLOUT�¼�
	4. ���Ӧ�ò㻺�����Ѿ����������ݷ��������ˣ�����Ҫȡ��ע��POLLOUT�¼�����Ȼ�����BUSY LOOP��æ�ȴ���
	5. ������acceptע��POLLOUT�¼���ֻ����Ҫ�������ݵ�ʱ��ע��POLLOUT�¼�����Ȼ�����BUSY LOOP��æ�ȴ���
	
	
ccept(2)����EMFILE�Ĵ����ļ���������Ŀ�������ƣ������û�м�ʱ�������ܻᷢ��BUSY LOOP��æ�ȴ���
1. ���߽����ļ���������Ŀ���������ţ�ϵͳ��Դ���кľ��������
2. ���ȣ��������ţ�
3. �˳����򣨲������ţ�
4. �رռ����׽��֣���ʲôʱ�����´��أ���
5. �����epollģ�ͣ����Ը�Ϊ���ش���ģ�ͣ�ET��ģʽ�����©����һ��accept���������Ҳ���ղ����µ����ӣ�
6. ׼��һ���µĿ��е��ļ�������
	�ȹر���������ļ������һ���ļ������������accept�õ�socket���ӵ��ļ����������������close������
	�����ŵĶϿ�����ͻ��˵����ӣ�������´���������ļ����ѡ��ӡ����ϣ��Ա�����ʹ��

*/


/**
��ET��ģʽ
	1. accept֮�󣬾�ע��POLLOUT�¼����������BUSY LOOP��æ�ȴ��������ɷ������ݵ��ɷ�������ֻ����һ��
	2. EOPLLIN�¼����ӵ͵�ƽ���ߵ�ƽ����--->�У������ݿɶ���ֻ����һ�Σ���һ��Ҫ�����е����ݶ����꣬������ջ��������˻���������read�������ؾͻ����EAGAIN�������û��ȫ��read���Ϳ�����Ҳûread��
		ֱ������EAGAIN����Ϊֹ���ܶ���
	3. �ں˻�����������ˣ��ʹ���͵�ƽ״̬�����ں˻������ճ���λ�ã��ͻ��ɸߵ�ƽ������EPOLLOUT�¼���Ӧ�ò�ͻᷢ�����ݣ�ֱ�����ݷ����꣬�����EAGAIN�������û��ȫ��������ϣ��Ϳ�����Ҳû���ᷢ��
	5. accept(2)����EMFILEʧ�ܣ���������δ�����Ͳ����ٴ���accept�¼��ˣ����ᷢ��BUSY LOOP��æ�ȴ��������������accept�׽��־���Ҳû����accept��
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

/// ��̬����
typedef std::vector<struct pollfd> PollFdList;

int main(void)
{
	/// ����ͻ��˹ر����׽���close,������������һ��write���������ͻ��յ�һ��RST��TCP����㣩
	/// ���������ٴε�����write������˾ͻ��յ�SIGPIPE�źţ�Ĭ�ϴ���ʽ�ǽ����˳�����������Ҫ��������ź�
	signal(SIGPIPE, SIG_IGN);
	
	/// ����������������close�����ڿͻ��˵���close�����������ͻ���������TIME_WAIT״̬
	/// Э������ϣ��ø��ÿͻ��������Ͽ����ӣ������Ͱ�TIME_WAIT��ɢ�������Ŀͻ�������ȥ��
	/// ����ͻ���һֱ���Ͽ�������������󣬾ͻ�һֱռ�÷���˵���Դ�����Է�����Ӧ����һ�����������Ͽ�����Ծ������
	signal(SIGCHLD, SIG_IGN);

	/// ����EMFILE���������ʱ�Ľ������
	//int idlefd = open("/dev/null", O_RDONLY | O_CLOEXEC);
	int listenfd;

	/// F_GETFD��ȡ�ļ���������״̬��F_SETFD�����ļ���������״̬
	//if ((listenfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
	/// SOCK_NONBLOCK �������׽���
	/// SOCK_CLOEXEC ���folkһ���ӽ���ʱ���ӽ���Ĭ�ϵ�״̬ʱ�ر�״̬
	if ((listenfd = socket(PF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP)) < 0)
		ERR_EXIT("socket");

	struct sockaddr_in servaddr;
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(5188);
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

	int on = 1;
	/// ���õ�ַ���ظ�����
	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
		ERR_EXIT("setsockopt");

	/// ���ļ�����������ַ
	if (bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
		ERR_EXIT("bind");
	
	/// ����
	if (listen(listenfd, SOMAXCONN) < 0)
		ERR_EXIT("listen");

	struct pollfd pfd;
	pfd.fd = listenfd;
	
	/// POLLIN �����ݿɶ�
	pfd.events = POLLIN;

	/// ���ļ���������ӵ���̬����
	PollFdList pollfds;
	pollfds.push_back(pfd);

	int nready;

	struct sockaddr_in peeraddr;
	socklen_t peerlen;
	int connfd;

	while (1)
	{
		/// ��C++11�� &*pollfds.begin() == pollfds.data()
		/// -1��Զ�ȴ�
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
			/// ���ʹ��acceptû�����������ļ��������Ĺ��ܣ�accept4֧��
			/// ���������ļ������� SOCK_NONBLOCK | SOCK_CLOEXEC �������׽��֣��ӽ���Ĭ�ϵ�״̬ʱ�ر�״̬
			connfd = accept4(listenfd, (struct sockaddr*)&peeraddr,
						&peerlen, SOCK_NONBLOCK | SOCK_CLOEXEC);

			if (connfd == -1)
				ERR_EXIT("accept4");

/*
		/// accept(2)����EMFILE�Ĵ���
		/// ��ʱ����ͻ������ŵĶϿ�����
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

			/// �������ӵ��׽��ּ��뵽�����Ķ�̬������
			pfd.fd = connfd;
			pfd.events = POLLIN;
			pfd.revents = 0;
			pollfds.push_back(pfd);
			--nready;

			// ���ӳɹ�
			std::cout<<"ip="<<inet_ntoa(peeraddr.sin_addr)<<
				" port="<<ntohs(peeraddr.sin_port)<<std::endl;
			if (nready == 0)
				continue;
		}

		//std::cout<<pollfds.size()<<std::endl;
		//std::cout<<nready<<std::endl;
		/// ��ѯ�����������׽��ַ������¼�
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
						/// ��C++vector�������У���ʹ��eraseʱ��������Ĭ�ϻ�ָ����һ��Ԫ�����ڵĵ�ַ�����������--����
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


