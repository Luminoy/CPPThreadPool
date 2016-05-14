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
	//�����߳�
	static DWORD WINAPI DefaultJobProc(LPVOID lpParam = NULL) {
		ThreadItem *pThread = static_cast<ThreadItem *>(lpParam);
		assert(pThread);
		ThreadPool *pThreadPoolObj = pThread->_pThis;
		assert(pThreadPoolObj);
		InterlockedIncrement(&pThreadPoolObj->_lThreadNum); //??? why
		HANDLE hWaitHandle[3]; //??? why
		hWaitHandle[0] = pThreadPoolObj->_SemaphoreCall;
		hWaitHandle[1] = pThreadPoolObj->_SemaphoreDel;
		hWaitHandle[2] = pThreadPoolObj->_EventEnd;
		JobItem *pJob;
		bool fHasJob;
		while (1) {
			DWORD wr = WaitForMultipleObjects(3, hWaitHandle, false, INFINITE);
			if (wr == WAIT_OBJECT_0 + 1) { //?? what's that??
				break;
			}
			//�Ӷ�����ȡ���û���ҵ
			EnterCriticalSection(&pThreadPoolObj->_csWorkQueue);
			if (fHasJob = !pThreadPoolObj->_JobQueue.empty()) {
				pJob = pThreadPoolObj->_JobQueue.front();
				pThreadPoolObj->_JobQueue.pop();
				assert(pJob);
			}
			LeaveCriticalSection(&pThreadPoolObj->_csWorkQueue);

			if (wr == WAIT_OBJECT_0 + 2 && !fHasJob) {
				break;
			}

			if (fHasJob && pJob) {
				InterlockedIncrement(&pThreadPoolObj->_lRunningNum); //????
				pThread->_dwLastBeginTime = GetTickCount();
				pThread->_dwCount++;
				pThread->_fIsRunning = true;
				pJob->_pFunc(pJob->_pParam);
				delete pJob;
				pThread->_fIsRunning = false;
				InterlockedDecrement(&pThreadPoolObj->_lRunningNum);
			}
		}
	}
	//�����û������麯��
	static void CallProc(void *pData) { 
		CallProcPara *cp = static_cast<CallProcPara *>(pData);
		assert(cp);
		if (cp) {
			cp->_pObj->DoJob(cp->_pParam);
			delete cp;
		}
	}
	struct CallProcPara //�û�����ṹ
	{
		ThreadJob *_pObj; //�û�����
		void *_pParam; //�û�����
		CallProcPara(ThreadJob *pObj, void *pParam) : _pObj(pObj), _pParam(pParam) {}
	};
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