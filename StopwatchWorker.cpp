#include "pch.h"
#include "StopwatchWorker.h"

StopwatchWorker::StopwatchWorker(CWnd* notifyWnd)
	: m_notifyWnd(notifyWnd)
	, m_event(FALSE, FALSE)
{
}

void StopwatchWorker::Start()
{
	m_exit = false;
	m_thread = AfxBeginThread(&StopwatchWorker::ThreadProc, this);
	if (m_thread) m_thread->m_bAutoDelete = FALSE;
}

void StopwatchWorker::Stop()
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

void StopwatchWorker::Enqueue(const Command& cmd)
{
	{
		CSingleLock lock(&m_queueCs, TRUE);
		m_queue.push_back(cmd);
	}
	m_event.SetEvent();
}

CString StopwatchWorker::GetNowText() const
{
	return m_stw.GetNowText();
}

CString StopwatchWorker::GetElapsedText()
{
	CSingleLock lock(&m_stateCs, TRUE);
	return m_stw.GetElapsedText();
}

bool StopwatchWorker::IsRunning()
{
	CSingleLock lock(&m_stateCs, TRUE);
	return m_stw.IsRunning();
}

void StopwatchWorker::SetResetPending(bool v)
{
	CSingleLock lock(&m_stateCs, TRUE);
	m_resetPending = v;
}

bool StopwatchWorker::GetResetPending()
{
	CSingleLock lock(&m_stateCs, TRUE);
	return m_resetPending;
}

UINT AFX_CDECL StopwatchWorker::ThreadProc(LPVOID pParam)
{
	auto self = reinterpret_cast<StopwatchWorker*>(pParam);
	self->ThreadLoop();
	return 0;
}

void StopwatchWorker::ApplyCommand(const Command& cmd)
{
	switch (cmd.kind)
	{
	case CommandKind::ToggleStartStop:
		m_resetPending = false;
		m_stw.ToggleStartStopAt(cmd.stamp);
		break;
	case CommandKind::Reset:
		m_stw.Reset();
		m_resetPending = false;
		break;
	default:
		break;
	}
}

void StopwatchWorker::ThreadLoop()
{
	CString lastNow;

	while (!m_exit)
	{
		BOOL signaled = m_event.Lock(16);
		if (m_exit) break;

		bool processedAny = false;
		bool postedLap = false;

		if (signaled)
		{
			for (;;)
			{
				Command cmd{};
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
				processedAny = true;

				if (cmd.kind == CommandKind::Lap)
				{
					Stopwatch::LapSnapshot snap;
					{
						CSingleLock lock(&m_stateCs, TRUE);
						snap = m_stw.LapAt(cmd.stamp);
					}

					if (snap.lapNo > 0)
					{
						auto payload = new LapPayload;
						payload->lapNo = snap.lapNo;
						payload->lapCounter = snap.lapCounter;

						{
							CSingleLock lock(&m_stateCs, TRUE);
							payload->lapText = m_stw.FormatCounter(snap.lapCounter);
							payload->totalText = m_stw.FormatCounter(snap.totalCounter);
						}

						if (m_notifyWnd)
						{
							if (m_notifyWnd->PostMessage(WM_APP_STW_LAP, (WPARAM)payload, 0))
								postedLap = true;
							else
								delete payload;
						}
						else
						{
							delete payload;
						}
					}
				}
				else
				{
					CSingleLock lock(&m_stateCs, TRUE);
					ApplyCommand(cmd);
				}
			}
		}

		bool running = false;
		{
			CSingleLock lock(&m_stateCs, TRUE);
			running = m_stw.IsRunning();
		}

		if (running || processedAny || postedLap)
		{
			if (m_notifyWnd)
				m_notifyWnd->PostMessage(WM_APP_STW_UPDATED, 0, 0);
			continue;
		}

		CString now = m_stw.GetNowText();
		if (now != lastNow)
		{
			lastNow = now;
			if (m_notifyWnd)
				m_notifyWnd->PostMessage(WM_APP_STW_UPDATED, 0, 0);
		}
	}
}
