////////////////////////////////////////////////////////////////////
///reference : http://blog.csdn.net/revv/article/details/3248424 ///
////////////////////////////////////////////////////////////////////

#ifndef _CPP_THREAD_POOL_H
#define _CPP_THREAD_POOL_H

#include <cassert>
#include <vector>
#include <queue>
#include <windows.h>

using namespace std;

class ThreadJob { //��������
public:
	virtual void DoJob(void *pParam) = 0;
};

class ThreadPool
{
public:
	//ThreadPool();
	ThreadPool(DWORD dwNum = 4);
	~ThreadPool();

	struct JobItem  //�û������ṹ
	{
		void(*_pFunc)(void *); //����ָ��
		void *_pParam; //����
		JobItem(void (*pFunc)(void *) = NULL, void *pParam = NULL) : _pFunc(pFunc), _pParam(pParam) {}
	};
	struct ThreadItem //�̳߳��е��߳̽ṹ
	{
		HANDLE _Handle; //�߳̾��
		ThreadPool *_pThis; //�̳߳ص�ָ��
		DWORD _dwLastBeginTime;	//���һ�����п�ʼʱ��
		DWORD _dwCount; //���д���
		bool _fIsRunning; //���б�־
		ThreadItem(ThreadPool *pthis) : _pThis(pthis), _Handle(NULL), _dwLastBeginTime(0), _dwCount(0), _fIsRunning(false) {}
		~ThreadItem() { if (_Handle) { CloseHandle(_Handle); _Handle = NULL; } }
	};
private:
	long _lThreadNum, _lRunningNum; //�߳���, ���е��߳���
	queue<JobItem *> _JobQueue; //��������
	vector<ThreadItem *> _ThreadVector; //�߳�����
	CRITICAL_SECTION _csThreadVector, _csWorkQueue; //���������ٽ�, �߳������ٽ�
	HANDLE _EventEnd, _EventComplete, _SemaphoreCall, _SemaphoreDel; //����֪ͨ, ����¼�, �����źţ� ɾ���߳��ź�
};

//ThreadPool::ThreadPool()
//{
//}

ThreadPool::ThreadPool(DWORD dwNum)
{
	_lThreadNum = _lRunningNum = 0;

}

ThreadPool::~ThreadPool()
{
}

int main() {
	return 0;
}
#endif // !_CPP_THREAD_POOL_H