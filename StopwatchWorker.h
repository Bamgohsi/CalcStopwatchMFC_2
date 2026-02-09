#pragma once

#include "Stopwatch.h"
#include "AppMessages.h"

#include <deque>
#include <atomic>
#include <afxmt.h>

// Stopwatch worker (thread/queue/sync)
class StopwatchWorker
{
public:
	enum class CommandKind
	{
		ToggleStartStop,
		Lap,
		Reset
	};

	struct Command
	{
		CommandKind kind;
		LONGLONG stamp;
	};

	struct LapPayload
	{
		int      lapNo = 0;
		LONGLONG lapCounter = 0;
		CString  lapText;
		CString  totalText;
	};

public:
	explicit StopwatchWorker(CWnd* notifyWnd);

	void Start();
	void Stop();

	void Enqueue(const Command& cmd);

	CString GetNowText() const;
	CString GetElapsedText();
	bool IsRunning();

	void SetResetPending(bool v);
	bool GetResetPending();

private:
	static UINT AFX_CDECL ThreadProc(LPVOID pParam);
	void ThreadLoop();
	void ApplyCommand(const Command& cmd);

private:
	CWnd* m_notifyWnd = nullptr;

	Stopwatch m_stw;

	CEvent m_event;
	CCriticalSection m_queueCs;
	CCriticalSection m_stateCs;
	std::deque<Command> m_queue;

	bool m_resetPending = false;

	std::atomic<bool> m_exit{ false };
	CWinThread* m_thread = nullptr;
};
