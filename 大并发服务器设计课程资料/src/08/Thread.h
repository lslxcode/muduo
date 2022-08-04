#ifndef _THREAD_H_
#define _THREAD_H_

#include <pthread.h>

class Thread
{
public:
	Thread();
	/// 基类析构的析构函数要用纯虚函数，不然会造成基类的资源无法释放
	virtual ~Thread();

	void Start();
	void Join();

	void SetAutoDelete(bool autoDelete);

private:
	/// 加了静态就没有隐含的this指针
	static void* ThreadRoutine(void* arg);
	virtual void Run() = 0;
	pthread_t threadId_;
	bool autoDelete_;
};

#endif // _THREAD_H_
