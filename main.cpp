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

//-----------------------------------------����Ϊ�����̺߳ͻָ��߳�ʹ�õľ�̬��----------------------------------------------------

//��¼�߳���Ϣ�Ľṹ��
typedef  struct _THREADINFO
{
	pthread_t tid;//�̵߳�id
	bool pending;//�ó�Ա���ڱ�ʶ����߳��Ƿ������ڹ�����
	pthread_mutex_t mut;//���ڹ�������߳�ʹ�õĻ�����
	pthread_cond_t cond;//���ڹ�������߳�ʹ�õ���������
}THREADINFO, *PTHREADINFO;

struct ThreadEqual {
public:
	bool operator()(const pthread_t& n1, const pthread_t& n2) const
	{
		return n1 == n2;
	}
};

//Ϊ�˿��ټ����̵߳���Ϣ�����ʹ��map������
typedef  unordered_map<pthread_t, PTHREADINFO, std::hash<pthread_t>, ThreadEqual> pthread_t_map;

class Thread
{
public:
	static pthread_t_map threads;
	static void thread_addinfo(pthread_t tid);
public:
	//���߳̽���ʱ��������Ը��̵߳���Ϣ
	static void thread_clearn();
	//�ָ�ĳһ���̣߳������������
	static bool thread_resume(pthread_t tid);
	//����ǰ�̣߳�ֱ�������ָ̻߳����߳�
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

//Ӧ�����������һ��Ϣ������δ��ĳһʱ��ϵͳ�������̵߳�id�ڴ˼�¼�У��Ϳ�����������
void Thread::thread_clearn()
{
	auto tid = pthread_self();
	if (Thread::threads.find(tid) != Thread::threads.end())
	{
		//���̵߳�ǰ�����ܴ��ڹ���״̬,�ɷ�������
		auto pthreadinfo = Thread::threads[tid];
		Thread::threads.erase(tid);
		pthread_mutex_destroy(&pthreadinfo->mut);
		pthread_cond_destroy(&pthreadinfo->cond);
		delete pthreadinfo;
	}
}

//�ָ�ĳһ���̣߳������������
bool Thread::thread_resume(pthread_t tid)
{

	if (Thread::threads.find(tid) == Thread::threads.end())return false;
	auto pthreadinfo = Thread::threads[tid];
	//���̴߳��ڹ�����ʱ�����ǿ��������������е�ĳ����϶����̹߳���Ļ�������Ȼ���޸����Ƿ����еı�־��Ȼ��
	pthread_mutex_lock(&pthreadinfo->mut);
	pthreadinfo->pending = false;
	pthread_cond_signal(&pthreadinfo->cond);
	pthread_mutex_unlock(&pthreadinfo->mut);
	return true;
}


//����ǰ�̣߳�ֱ�������ָ̻߳����̣߳����̲߳Ż�ָ����У�
bool Thread::thread_pause()
{
	auto tid = pthread_self();

	thread_addinfo(tid);
	if (Thread::threads.find(tid) == Thread::threads.end())return false;

	auto pthreadinfo = Thread::threads[tid];
	pthread_mutex_lock(&pthreadinfo->mut);
	pthreadinfo->pending = true;
	while (pthreadinfo->pending)//ֻҪpendingΪtrue�����ó���һֱ�ȴ���ȥ(����ĵȴ����ǹ���)
	{
		//�������ִ�й�����������
		//1.�ͷŻ����� 
		//2.�̴߳��ڵȴ�״̬��ֱ��������������������߳�ʹ��pthread_cond_signal(&cond)�������������� 
		//3.������������
		//ע�⣺�߳�����ȴ��׶������߳̿��Ի�û��������޸�pending��ֵ��Ȼ�����ͷŻ�����
		pthread_cond_wait(&pthreadinfo->cond, &pthreadinfo->mut);
	}
	pthread_mutex_unlock(&pthreadinfo->mut);
	return true;
}





//----------------------------------------------------------------����Ϊ��Ϣ���е���-----------------------------------------


//������Ϣ�Ľṹ��
typedef struct _MSG
{
	int type;//��Ϣ������
	int parameter_type;//�������ݵ���������
	bool self_free;//ָʾdata��Ա��Ӧ�Ļ������Ƿ���Ҫ������Ϣ���߳��ͷ�
	int  datalength;//data��Աָʾ�Ļ������ĳ���
	void *data;//������Ϣ�Ļ�����
}MSG, *PMSG;

//��Ϣ���͵�ö��
enum msg_type
{
	a,/*��ӡ������*/
	b,/*�߳��˳�*/
	c,
	d,
	e,
	f,
	g
};

typedef void *(*PFPROC) (void *);//�����߳�ʹ�õ��̺߳�������ָ����������


//�����̼߳以�෢����Ϣ���࣬����ά����Ϣ���в����̼߳��Ч�ķ�����Ϣ��
//ʹ�����̣�
//1.����һ������A
//2.����A.start����
//3.1��̬����һ��MSG����Ȼ���ʼ�������Ϣ����
//3.2��һ���߳��е���A.add_msg��������Ϣ�����������Ϣ�����Ϣ����
//4.1��ThreadMsg����������߳��е���A.get_msg���������Ϣ(�����߳�Ҳ���Ե���������������Ϣ�����е���Ϣ)
//4.2��ThreadMsg����������߳��д��������Ϣ
//4.3��ThreadMsg����������߳����ͷŻ�ȡ������Ϣ���ڴ�
//5.��ThreadMsg����������߳��˳�ʱ����A.thread_close����
class ThreadMsg
{
public:
	bool running;//ָʾ�����Ϣ�����Ƿ���Թ���
	PFPROC proc;//���ڴ����߳�ʱ��ʱ�����̺߳�����ַ�ı���
	pthread_t tid;//��Ϣ���й������̵߳�id
	queue<PMSG> msgq;//��Ϣ����
	pthread_mutex_t mutex_lock;//������Ϣ����ʹ�õĻ�����
	vector<pthread_t> wait_threads;//��ǰΪ�˻�ȡ��Ϣ�����ڹ����е��̣߳���Щ�߳�һ����ѡ���˵ȴ���
public:
	ThreadMsg(pthread_t tid);
	ThreadMsg(void *(*__start_routine) (void *));
	~ThreadMsg();
	//��ǰ����Ϣ���������һ����Ϣ
	bool add_msg(PMSG pmsg);
	//����Ϣ������ȡ��һ����Ϣ
	//wait����ΪTrue����������ʱһ������һ����Ϣ��ע�⺯����û����֮ǰ�����ñ��������߳̽�һֱ����������
	//wait����ΪFalse���������������أ�����һ����Ϣ���ɹ��Ĵ���Ϣ������ȡ��һ����Ϣ���򷵻�NULL����ǰ��Ϣ������û����Ϣ��;
	PMSG get_msg(bool wait);
	//������Ϣ����
	bool start();
	//��ù����̵߳�tid
	pthread_t get_tid();
	//�������߳̽���ʱ������õĺ���
	void thread_close();
};

//����һ����Ϣ�������ڽ����ڵĸ����߳�ͨ��
//������pthread_t	�̵߳�id����ʾ�����Ϣ����Ĭ�Ϲ������̣߳�ʹ��ʱ���ÿ��ǹ����ԣ���Ϊ֧�������̴߳������Ϣ������ȡ����Ϣ��
ThreadMsg::ThreadMsg(pthread_t tid)
{
	this->tid = tid;
	proc = NULL;
	running = false;
}

//����һ����Ϣ�������ڽ����ڵĸ����߳�ͨ�ţ������Ĭ�ϴ���һ���߳�
//������thread_proc	�̵߳���ʼ������ַ����ʾ�����Ϣ����Ĭ�Ϲ������̣߳�
ThreadMsg::ThreadMsg(void *(*thread_proc) (void *))
{
	proc = thread_proc;
	running = false;
}

ThreadMsg::~ThreadMsg()
{
	running = false;
	pthread_mutex_lock(&mutex_lock);//�ͷ���Ϣ������δ�������Ϣ
	while (msgq.size())
	{
		auto pmsg = msgq.front();
		msgq.pop();
		if (pmsg->self_free)
			free(pmsg->data);
		free(pmsg);
	}
	pthread_mutex_unlock(&mutex_lock);
	pthread_mutex_destroy(&mutex_lock);//�ͷ���Դ
}

//������Ϣ����
bool ThreadMsg::start()
{
	pthread_mutex_init(&mutex_lock, NULL);//��ʼ���߳���Ϣ����ʹ�õĻ�����
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

//��ǰ����Ϣ���������һ����Ϣ
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

//����Ϣ������ȡ��һ����Ϣ
//wait����Ϊtrue����������ʱһ������һ����Ϣ��ע�⺯����û����֮ǰ�����ñ��������߳̽�һֱ����������
//wait����Ϊfalse���������������أ�����һ����Ϣ���ɹ��Ĵ���Ϣ������ȡ��һ����Ϣ���򷵻�NULL����ǰ��Ϣ������û����Ϣ��;
PMSG ThreadMsg::get_msg(bool wait)
{
	if (!running) return NULL;
	//�Ȳ鿴�Ƿ�����Ϣ�������ֱ��ȡ����Ϣ������
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
	//��ǰ��Ϣ����������Ϣ
	if (!wait)return ret;//�����Ը��ȴ�ֱ�ӷ���
	//��ǰ��Ϣ�����ǿգ������߳�Ը��ȴ���ֱ����Ϣ������ܷ���
	auto tid = pthread_self();
	wait_threads.push_back(tid);//����ǰ�߳���ӵ��ȴ���ǰ���е�������
	Thread::thread_pause();//����ǰ�߳�,ֱ������̱߳��ָ�Ϊֹ
	goto A;
}

//��ù����̵߳�tid
pthread_t ThreadMsg::get_tid()
{
	return tid;
}

//��Ϣ���й������߳��ڽ���ǰӦ�����õĺ�������������̵߳���Ϣ
void ThreadMsg::thread_close()
{
	Thread::thread_clearn();
}
//-----------------------------------------------------------����Ϊ���Դ���------------------------------------------------------------

ThreadMsg* t1;//ȫ�ֱ����������̼߳乲��

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
			case a://������
				//��������������
				printf("msg type:%d\n", a);
				//�ͷ���Ϣ�ڲ��ṩ�Ļ�����
				if (pMsg->self_free&&pMsg->data)free(pMsg->data);
				//�ͷ�Msgռ�õ��ڴ�
				free(pMsg);
				break;
			case b:
				//��������������
				printf("msg type:%d\n", b);
				//�ͷ���Ϣ�ڲ��ṩ�Ļ�����
				if (pMsg->self_free&&pMsg->data)free(pMsg->data);
				//�ͷ�Msgռ�õ��ڴ�
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

	//���߳�1����Ϣ���������һ����Ϣ
	auto pmsg = (PMSG)malloc(sizeof(MSG));
	memset(pmsg, 0, sizeof(MSG));
	pmsg->data = NULL;
	pmsg->type = a;
	pmsg->self_free = true;
	pmsg->datalength = 0;
	pmsg->parameter_type = 0;
	t1->add_msg(pmsg);
	//���߳�1����Ϣ���������һ����Ϣ
	auto pmsg1 = (PMSG)malloc(sizeof(MSG));
	memset(pmsg1, 0, sizeof(MSG));
	pmsg1->type = b;
	t1->add_msg(pmsg1);
	printf("Main thread is runing.\n");
	pthread_join(t1->get_tid(), NULL);//�ȴ����߳��������˳�
	//�ͷŶ�̬�����Ķ���
	delete t1;
	t1 = NULL;
	printf("Main thread is finfshed.\n");
	return 0;
}
