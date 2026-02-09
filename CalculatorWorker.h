#pragma once

#include "Calculator.h"
#include "AppMessages.h"

#include <deque>
#include <atomic>
#include <afxmt.h>

// Calculator worker (thread/queue/sync)
class CalculatorWorker
{
public:
	explicit CalculatorWorker(CWnd* notifyWnd);

	void Start();
	void Stop();

	void Enqueue(const Calculator::Command& cmd);

	CString GetDisplay();
	CString GetHistory();

private:
	static UINT AFX_CDECL ThreadProc(LPVOID pParam);
	void ThreadLoop();

private:
	CWnd* m_notifyWnd = nullptr;

	Calculator m_calc;

	CEvent m_event;
	CCriticalSection m_queueCs;
	CCriticalSection m_stateCs;
	std::deque<Calculator::Command> m_queue;

	std::atomic<bool> m_exit{ false };
	CWinThread* m_thread = nullptr;
};
