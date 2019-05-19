#include <cstdio>
#include <queue>
#include <iostream>
#include <string.h>
#include <unistd.h>
#include <unordered_map>
#include <queue>
#include <vector>
#include <pthread.h>
using namespace std;

//-----------------------------------------以下为挂起线程和恢复线程使用的静态类----------------------------------------------------

//记录线程信息的结构体
typedef  struct _THREADINFO
{
	pthread_t tid;//线程的id
	bool pending;//该成员用于标识这个线程是否正处于挂起中
	pthread_mutex_t mut;//用于挂起这个线程使用的互斥锁
	pthread_cond_t cond;//用于挂起这个线程使用的条件变量
}THREADINFO, *PTHREADINFO;

struct ThreadEqual {
public:
	bool operator()(const pthread_t& n1, const pthread_t& n2) const
	{
		return n1 == n2;
	}
};

//为了快速检索线程的信息，因此使用map来保存
typedef  unordered_map<pthread_t, PTHREADINFO, std::hash<pthread_t>, ThreadEqual> pthread_t_map;

class Thread
{
public:
	static pthread_t_map threads;
	static void thread_addinfo(pthread_t tid);
public:
	//当线程结束时，清理本类对该线程的信息
	static void thread_clearn();
	//恢复某一个线程，让其继续运行
	static bool thread_resume(pthread_t tid);
	//挂起当前线程，直到其他线程恢复本线程
	static bool thread_pause();
};

pthread_t_map Thread::threads;

void Thread::thread_addinfo(pthread_t tid)
{
	if (Thread::threads.find(tid) == Thread::threads.end())
	{
		auto pthreadinfo = new THREADINFO();
		pthreadinfo->pending = false;
		pthreadinfo->tid = tid;
		pthread_mutex_init(&pthreadinfo->mut, NULL);
		pthread_cond_init(&pthreadinfo->cond, NULL);
		Thread::threads.emplace(tid, pthreadinfo);
	}
}

//应当定期清除这一信息，否则未来某一时刻系统创建了线程的id在此记录中，就可能引发错误
void Thread::thread_clearn()
{
	auto tid = pthread_self();
	if (Thread::threads.find(tid) != Thread::threads.end())
	{
		//本线程当前不可能处于挂起状态,可放心销毁
		auto pthreadinfo = Thread::threads[tid];
		Thread::threads.erase(tid);
		pthread_mutex_destroy(&pthreadinfo->mut);
		pthread_cond_destroy(&pthreadinfo->cond);
		delete pthreadinfo;
	}
}

//恢复某一个线程，让其继续运行
bool Thread::thread_resume(pthread_t tid)
{

	if (Thread::threads.find(tid) == Thread::threads.end())return false;
	auto pthreadinfo = Thread::threads[tid];
	//当线程处于挂起中时，我们可以在其挂起过程中的某个间隙获得线程挂起的互斥锁，然后修改其是否运行的标志，然后
	pthread_mutex_lock(&pthreadinfo->mut);
	pthreadinfo->pending = false;
	pthread_cond_signal(&pthreadinfo->cond);
	pthread_mutex_unlock(&pthreadinfo->mut);
	return true;
}


//挂起当前线程（直到其他线程恢复本线程，本线程才会恢复运行）
bool Thread::thread_pause()
{
	auto tid = pthread_self();

	thread_addinfo(tid);
	if (Thread::threads.find(tid) == Thread::threads.end())return false;

	auto pthreadinfo = Thread::threads[tid];
	pthread_mutex_lock(&pthreadinfo->mut);
	pthreadinfo->pending = true;
	while (pthreadinfo->pending)//只要pending为true，就让程序一直等待下去(这里的等待就是挂起)
	{
		//这个函数执行共有三个步骤
		//1.释放互斥锁 
		//2.线程处于等待状态，直到条件变量被激活（其他线程使用pthread_cond_signal(&cond)激活条件变量） 
		//3.锁定互斥锁。
		//注意：线程在其等待阶段其他线程可以获得互斥锁并修改pending的值，然后再释放互斥锁
		pthread_cond_wait(&pthreadinfo->cond, &pthreadinfo->mut);
	}
	pthread_mutex_unlock(&pthreadinfo->mut);
	return true;
}





//----------------------------------------------------------------以下为消息队列的类-----------------------------------------


//保存消息的结构体
typedef struct _MSG
{
	int type;//消息的类型
	int parameter_type;//附加数据的数据类型
	bool self_free;//指示data成员对应的缓冲区是否需要处理消息的线程释放
	int  datalength;//data成员指示的缓冲区的长度
	void *data;//保存消息的缓冲区
}MSG, *PMSG;

//消息类型的枚举
enum msg_type
{
	a,/*打印买泡面*/
	b,/*线程退出*/
	c,
	d,
	e,
	f,
	g
};

typedef void *(*PFPROC) (void *);//创建线程使用的线程函数函数指针数据类型


//用于线程间互相发送消息的类，该类维护消息队列并让线程间高效的发送消息。
//使用流程：
//1.创建一个对象A
//2.调用A.start函数
//3.1动态创建一个MSG对象，然后初始化这个消息对象
//3.2在一个线程中调用A.add_msg函数向消息队列中添加消息这个消息对象
//4.1在ThreadMsg对象关联的线程中调用A.get_msg函数获得消息(其它线程也可以调用这个函数获得消息队列中的消息)
//4.2在ThreadMsg对象关联的线程中处理这个消息
//4.3在ThreadMsg对象关联的线程中释放获取到的消息的内存
//5.在ThreadMsg对象关联的线程退出时调用A.thread_close函数
class ThreadMsg
{
public:
	bool running;//指示这个消息队列是否可以工作
	PFPROC proc;//用于创建线程时临时保存线程函数地址的变量
	pthread_t tid;//消息队列关联的线程的id
	queue<PMSG> msgq;//消息队列
	pthread_mutex_t mutex_lock;//访问消息队列使用的互斥锁
	vector<pthread_t> wait_threads;//当前为了获取消息而处于挂起中的线程（这些线程一定是选择了等待）
public:
	ThreadMsg(pthread_t tid);
	ThreadMsg(void *(*__start_routine) (void *));
	~ThreadMsg();
	//向当前的消息队列中添加一个消息
	bool add_msg(PMSG pmsg);
	//从消息队列中取出一个消息
	//wait参数为True：函数返回时一定返回一个消息。注意函数在没返回之前，调用本函数的线程将一直处于阻塞中
	//wait参数为False：函数将立即返回，返回一个消息（成功的从消息队列中取出一个消息）或返回NULL（当前消息队列中没有消息）;
	PMSG get_msg(bool wait);
	//启动消息队列
	bool start();
	//获得关联线程的tid
	pthread_t get_tid();
	//关联的线程结束时必须调用的函数
	void thread_close();
};

//创建一个消息队列用于进程内的各个线程通信
//参数：pthread_t	线程的id（表示这个消息队列默认关联的线程，使用时不用考虑关联性，因为支持其他线程从这个消息队列中取出消息）
ThreadMsg::ThreadMsg(pthread_t tid)
{
	this->tid = tid;
	proc = NULL;
	running = false;
}

//创建一个消息队列用于进程内的各个线程通信，这里会默认创建一个线程
//参数：thread_proc	线程的起始函数地址（表示这个消息队列默认关联的线程）
ThreadMsg::ThreadMsg(void *(*thread_proc) (void *))
{
	proc = thread_proc;
	running = false;
}

ThreadMsg::~ThreadMsg()
{
	running = false;
	pthread_mutex_lock(&mutex_lock);//释放消息队列中未清除的消息
	while (msgq.size())
	{
		auto pmsg = msgq.front();
		msgq.pop();
		if (pmsg->self_free)
			free(pmsg->data);
		free(pmsg);
	}
	pthread_mutex_unlock(&mutex_lock);
	pthread_mutex_destroy(&mutex_lock);//释放资源
}

//启动消息队列
bool ThreadMsg::start()
{
	pthread_mutex_init(&mutex_lock, NULL);//初始化线程消息队列使用的互斥锁
	if (proc == NULL && tid != 0)
	{
		running = true;
		return true;
	}
	if (proc != NULL && 0 == pthread_create(&this->tid, NULL, proc, NULL))
	{
		running = true;
		return true;
	}
	return false;
}

//向当前的消息队列中添加一个消息
bool ThreadMsg::add_msg(PMSG pmsg)
{
	if (!running) return false;
	pthread_mutex_lock(&mutex_lock);
	if (pmsg != NULL)msgq.push(pmsg);
	if (wait_threads.size() > 0)
	{
		Thread::thread_resume(wait_threads.back());
		wait_threads.pop_back();
	}
	pthread_mutex_unlock(&mutex_lock);
}

//从消息队列中取出一个消息
//wait参数为true：函数返回时一定返回一个消息。注意函数在没返回之前，调用本函数的线程将一直处于阻塞中
//wait参数为false：函数将立即返回，返回一个消息（成功的从消息队列中取出一个消息）或返回NULL（当前消息队列中没有消息）;
PMSG ThreadMsg::get_msg(bool wait)
{
	if (!running) return NULL;
	//先查看是否有消息，如果有直接取出消息并返回
	PMSG ret = NULL;
A:
	pthread_mutex_lock(&mutex_lock);
	if (msgq.size() > 0)
	{
		ret = msgq.front();
		msgq.pop();
	}
	pthread_mutex_unlock(&mutex_lock);
	if (ret != NULL)return ret;
	//当前消息队列中无消息
	if (!wait)return ret;//如果不愿意等待直接返回
	//当前消息队列是空，但是线程愿意等待，直到消息到达才能返回
	auto tid = pthread_self();
	wait_threads.push_back(tid);//将当前线程添加到等待当前队列的向量中
	Thread::thread_pause();//挂起当前线程,直到这个线程被恢复为止
	goto A;
}

//获得关联线程的tid
pthread_t ThreadMsg::get_tid()
{
	return tid;
}

//消息队列关联的线程在结束前应当调用的函数，用于清除线程的信息
void ThreadMsg::thread_close()
{
	Thread::thread_clearn();
}
//-----------------------------------------------------------以下为测试代码------------------------------------------------------------

ThreadMsg* t1;//全局变量，用于线程间共享

void *sub_thread_proc1(void *arg)
{
	printf("subthread is running.\n");
	while (true)
	{
		auto pMsg = t1->get_msg(true);
		if (pMsg != NULL)
		{
			switch (pMsg->type)
			{
			case a://买泡面
				//做你想做的事情
				printf("msg type:%d\n", a);
				//释放消息内部提供的缓冲区
				if (pMsg->self_free&&pMsg->data)free(pMsg->data);
				//释放Msg占用的内存
				free(pMsg);
				break;
			case b:
				//做你想做的事情
				printf("msg type:%d\n", b);
				//释放消息内部提供的缓冲区
				if (pMsg->self_free&&pMsg->data)free(pMsg->data);
				//释放Msg占用的内存
				free(pMsg);
				printf("sub thread is finished.\n");
				t1->thread_close();
				return NULL;
				break;
			}
		}
	}
	return arg;
}


int main()
{
	t1 = new ThreadMsg(sub_thread_proc1);
	t1->start();

	//向线程1的消息队列中添加一条消息
	auto pmsg = (PMSG)malloc(sizeof(MSG));
	memset(pmsg, 0, sizeof(MSG));
	pmsg->data = NULL;
	pmsg->type = a;
	pmsg->self_free = true;
	pmsg->datalength = 0;
	pmsg->parameter_type = 0;
	t1->add_msg(pmsg);
	//向线程1的消息队列中添加一条消息
	auto pmsg1 = (PMSG)malloc(sizeof(MSG));
	memset(pmsg1, 0, sizeof(MSG));
	pmsg1->type = b;
	t1->add_msg(pmsg1);
	printf("Main thread is runing.\n");
	pthread_join(t1->get_tid(), NULL);//等待子线程正常的退出
	//释放动态创建的对象
	delete t1;
	t1 = NULL;
	printf("Main thread is finfshed.\n");
	return 0;
}
