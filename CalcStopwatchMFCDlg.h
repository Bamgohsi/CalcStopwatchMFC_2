// CalcStopwatchMFCDlg.h: 헤더 파일
//

#pragma once

#include "Calculator.h"
#include "Stopwatch.h"

#include <deque>
#include <afxmt.h>

// 워커 스레드 -> UI 갱신 메시지
#define WM_APP_CALC_UPDATED (WM_APP + 1)
#define WM_APP_STW_UPDATED  (WM_APP + 2)
#define WM_APP_STW_LAP      (WM_APP + 3)


// CCalcStopwatchMFCDlg
class CCalcStopwatchMFCDlg : public CDialogEx
{
public:
	CCalcStopwatchMFCDlg(CWnd* pParent = nullptr);

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_CALCSTOPWATCHMFC_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);


protected:
	HICON m_hIcon;

	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnDestroy();
	DECLARE_MESSAGE_MAP()

	virtual BOOL PreTranslateMessage(MSG* pMsg);

protected:
	// 계산기 버튼 통합 핸들러
	afx_msg void OnCalcButtonRange(UINT nID);

	// 스톱워치 버튼
	afx_msg void OnStwPlayStop();
	afx_msg void OnStwRecord();
	afx_msg void OnStwClear();

	// 스레드 메시지 핸들러
	afx_msg LRESULT OnCalcUpdated(WPARAM, LPARAM);
	afx_msg LRESULT OnStwUpdated(WPARAM, LPARAM);
	afx_msg LRESULT OnStwLap(WPARAM, LPARAM);

private:
	Calculator m_calc;
	Stopwatch  m_stw;

	CListCtrl m_lapList;
	CFont m_calcResultFont;
	CFont m_stwResultFont;
	CFont m_stwLabelFont;

	// 스레드 동기화
	CEvent m_calcEvent;
	CEvent m_stwEvent;

	CCriticalSection m_calcQueueCs;
	CCriticalSection m_calcStateCs;
	CCriticalSection m_stwQueueCs;
	CCriticalSection m_stwStateCs;

	// UI -> 계산기 스레드 명령 큐
	std::deque<Calculator::Command> m_calcQueue;

	// 스톱워치 명령
	enum class SwCmdKind
	{
		ToggleStartStop,
		Lap,
		Reset
	};

	struct SwCommand
	{
		SwCmdKind kind;
		LONGLONG  stamp;
	};

	// UI -> 스톱워치 스레드 명령 큐
	std::deque<SwCommand> m_stwQueue;

	bool m_calcExit = false;
	bool m_stwExit = false;

	CWinThread* m_calcThread = nullptr;
	CWinThread* m_stwThread = nullptr;

	// 스레드 엔트리
	static UINT AFX_CDECL CalcThreadProc(LPVOID pParam);
	static UINT AFX_CDECL StwThreadProc(LPVOID pParam);

	// 명령 큐 삽입
	void EnqueueCalc(const Calculator::Command& cmd);
	void EnqueueStw(const SwCommand& cmd);
	void ApplyStwCommand(const SwCommand& cmd);

	// 화면 갱신
	void UpdateCalcUI();
	void UpdateStopwatchUI();

	// 랩타임 데이터
	struct LapRow
	{
		int      lapNo = 0;
		LONGLONG lapCounter = 0;
		CString  lapText;
		CString  totalText;
	};

	// 스레드 간 전달용
	struct LapPayload
	{
		int      lapNo = 0;
		LONGLONG lapCounter = 0;
		CString  lapText;
		CString  totalText;
	};

	std::deque<LapRow> m_laps;
	void RebuildLapList();

	bool m_stwEverStarted = false;
	bool m_stwResetPending = false;

	// UI 캐시
	CString m_lastCalcDisp;
	CString m_lastCalcHist;
	CString m_lastNow;
	CString m_lastElapsed;
	BOOL m_lastRcEnabled = (BOOL)-1;
	BOOL m_lastClearEnabled = (BOOL)-1;
};
