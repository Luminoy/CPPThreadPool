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

class ThreadJob { //工作基类
public:
	virtual void DoJob(void *pParam) = 0;
};

class ThreadPool
{
public:
	//ThreadPool();
	ThreadPool(DWORD dwNum = 4) : _lThreadNum(0),  _lRunningNum(0) 
	{
		InitializeCriticalSection(&_csThreadVector);
		InitializeCriticalSection(&_csWorkQueue);
		_EventComplete = CreateEvent(0, false, false, NULL);
		_EventEnd = CreateEvent(0, true, false, NULL);
		_SemaphoreCall = CreateSemaphore(0, 0, 0x7FFFFFFF, NULL);
		_SemaphoreDel = CreateSemaphore(0, 0, 0x7FFFFFFF, NULL);
		assert(_SemaphoreCall != INVALID_HANDLE_VALUE);
		assert(_SemaphoreDel != INVALID_HANDLE_VALUE);
		assert(_EventComplete != INVALID_HANDLE_VALUE);
		assert(_EventEnd != INVALID_HANDLE_VALUE);
		AdjustSize(dwNum <= 0 ? 4 : dwNum);
	}

	//调整线程池规模
	int AdjustSize(int iNum) {
		if (iNum > 0) {
			ThreadItem *pNew;
			EnterCriticalSection(&_csThreadVector);
			for (int i = 0; i < iNum; i++) {
				_ThreadVector.push_back(pNew = new ThreadItem(this));
				assert(pNew);
				pNew->_Handle = CreateThread(NULL, 0, DefaultJobProc, pNew, 0, NULL);

				SetThreadPriority(pNew->_Handle, THREAD_PRIORITY_BELOW_NORMAL);
				assert(pNew->_Handle);
			}
			LeaveCriticalSection(&_csThreadVector);
		}
		else
		{
			iNum *= -1;
			ReleaseSemaphore(_SemaphoreDel, iNum > _lThreadNum ? _lThreadNum : iNum, NULL);
		}
		return (int)_lThreadNum;
	}

	~ThreadPool() {
		DeleteCriticalSection(&_csWorkQueue);
		CloseHandle(_EventComplete);
		CloseHandle(_EventEnd);
		CloseHandle(_SemaphoreCall);
		CloseHandle(_SemaphoreDel);

		vector<ThreadItem *>::iterator iter;
		for (iter = _ThreadVector.begin(); iter != _ThreadVector.end(); iter++) {
			if (*iter)	delete *iter;
		}
		DeleteCriticalSection(&_csThreadVector);
	}
	//工作线程
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
			//从队列中取得用户作业
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
		EnterCriticalSection(&pThreadPoolObj->_csThreadVector);
		pThreadPoolObj->_ThreadVector.erase(find(pThreadPoolObj->_ThreadVector.begin(), pThreadPoolObj->_ThreadVector.end(), pThread));
		LeaveCriticalSection(&pThreadPoolObj->_csThreadVector);
		delete pThread;
		InterlockedDecrement(&pThreadPoolObj->_lThreadNum);
		if (!pThreadPoolObj->_lThreadNum) {
			SetEvent(pThreadPoolObj->_EventComplete);
		}
		return 0;
	}
	//调用线程池
	void Call(void(*pFunc)(void *), void *pParam = NULL) {
		assert(pFunc);
		EnterCriticalSection(&_csWorkQueue);
		_JobQueue.push(new JobItem(pFunc, pParam));
		LeaveCriticalSection(&_csWorkQueue);
		ReleaseSemaphore(_SemaphoreCall, 1, NULL);
	}
	//调用线程池
	inline void Call(ThreadJob *p, void *pParam = NULL) {
		Call(CallProc, new CallProcPara(p, pParam));
	}
	inline void End() {
		SetEvent(_EventEnd);
	}
	inline DWORD Size() {
		return (DWORD)_lThreadNum;
	}
	inline DWORD GetRunningSize() {
		return (DWORD)_lRunningNum;
	}
	bool IsRunning() {
		return _lRunningNum > 0;
	}

	//调用用户对象虚函数
	static void CallProc(void *pData) { 
		CallProcPara *cp = static_cast<CallProcPara *>(pData);
		assert(cp);
		if (cp) {
			cp->_pObj->DoJob(cp->_pParam);
			delete cp;
		}
	}
	struct CallProcPara //用户对象结构
	{
		ThreadJob *_pObj; //用户对象
		void *_pParam; //用户参数
		CallProcPara(ThreadJob *pObj, void *pParam) : _pObj(pObj), _pParam(pParam) {}
	};
	struct JobItem  //用户函数结构
	{
		void(*_pFunc)(void *); //函数指针
		void *_pParam; //参数
		JobItem(void (*pFunc)(void *) = NULL, void *pParam = NULL) : _pFunc(pFunc), _pParam(pParam) {}
	};
	struct ThreadItem //线程池中的线程结构
	{
		HANDLE _Handle; //线程句柄
		ThreadPool *_pThis; //线程池的指针
		DWORD _dwLastBeginTime;	//最后一次运行开始时间
		DWORD _dwCount; //运行次数
		bool _fIsRunning; //运行标志
		ThreadItem(ThreadPool *pthis) : _pThis(pthis), _Handle(NULL), _dwLastBeginTime(0), _dwCount(0), _fIsRunning(false) {}
		~ThreadItem() { if (_Handle) { CloseHandle(_Handle); _Handle = NULL; } }
	};
private:
	long _lThreadNum, _lRunningNum; //线程数, 运行的线程数
	queue<JobItem *> _JobQueue; //工作队列
	vector<ThreadItem *> _ThreadVector; //线程数据
	CRITICAL_SECTION _csThreadVector, _csWorkQueue; //工作队列临界, 线程数据临界
	HANDLE _EventEnd, _EventComplete, _SemaphoreCall, _SemaphoreDel; //结束通知, 完成事件, 工作信号， 删除线程信号
};

//ThreadPool::ThreadPool()
//{
//}

ThreadPool::~ThreadPool()
{
}

void threadfunc(void *p) {
	int *myVal = (int *)p;
	
}

#endif // !_CPP_THREAD_POOL_H

int main() {
	ThreadPool tp(20);
	for (int i = 0; i < 1000; i++) {
		tp.Call(threadfunc, &i);
	}
	return 0;
}