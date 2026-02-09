#include "pch.h"
#include "CalculatorWorker.h"

CalculatorWorker::CalculatorWorker(CWnd* notifyWnd)
	: m_notifyWnd(notifyWnd)
	, m_event(FALSE, FALSE)
{
}

void CalculatorWorker::Start()
{
	m_exit = false;
	m_thread = AfxBeginThread(&CalculatorWorker::ThreadProc, this);
	if (m_thread) m_thread->m_bAutoDelete = FALSE;
}

void CalculatorWorker::Stop()
{
	m_exit = true;
	m_event.SetEvent();

	if (m_thread)
	{
		::WaitForSingleObject(m_thread->m_hThread, INFINITE);
		delete m_thread;
		m_thread = nullptr;
	}
}

void CalculatorWorker::Enqueue(const Calculator::Command& cmd)
{
	{
		CSingleLock lock(&m_queueCs, TRUE);
		m_queue.push_back(cmd);
	}
	m_event.SetEvent();
}

CString CalculatorWorker::GetDisplay()
{
	CSingleLock lock(&m_stateCs, TRUE);
	return m_calc.GetDisplay();
}

CString CalculatorWorker::GetHistory()
{
	CSingleLock lock(&m_stateCs, TRUE);
	return m_calc.GetRSHS();
}

UINT AFX_CDECL CalculatorWorker::ThreadProc(LPVOID pParam)
{
	auto self = reinterpret_cast<CalculatorWorker*>(pParam);
	self->ThreadLoop();
	return 0;
}

void CalculatorWorker::ThreadLoop()
{
	while (!m_exit)
	{
		m_event.Lock();
		if (m_exit) break;

		for (;;)
		{
			Calculator::Command cmd{};
			bool hasCmd = false;

			{
				CSingleLock lock(&m_queueCs, TRUE);
				if (!m_queue.empty())
				{
					cmd = m_queue.front();
					m_queue.pop_front();
					hasCmd = true;
				}
			}

			if (!hasCmd) break;

			{
				CSingleLock lock(&m_stateCs, TRUE);
				m_calc.Execute(cmd);
			}
		}

		if (m_notifyWnd)
			m_notifyWnd->PostMessage(WM_APP_CALC_UPDATED, 0, 0);
	}
}
