// CalcStopwatchMFCDlg.h: 헤더 파일
//

#pragma once

#include "Calculator.h"
#include "Stopwatch.h"

#include <deque>
#include <afxmt.h>

// 작업 스레드 -> UI 스레드로 화면 갱신을 요청하는 사용자 정의 메시지
#define WM_APP_CALC_UPDATED (WM_APP + 1)
#define WM_APP_STW_UPDATED  (WM_APP + 2)
#define WM_APP_STW_LAP      (WM_APP + 3)


// CCalcStopwatchMFCDlg 대화 상자
class CCalcStopwatchMFCDlg : public CDialogEx
{
	// 생성입니다.
public:
	CCalcStopwatchMFCDlg(CWnd* pParent = nullptr);	// 표준 생성자입니다.

// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_CALCSTOPWATCHMFC_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 지원입니다.


// 구현입니다.
protected:
	HICON m_hIcon;

	// 생성된 메시지 맵 함수
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnDestroy();
	DECLARE_MESSAGE_MAP()

	// [키보드] 메시지를 가로채서 단축키를 처리하기 위한 가상함수
	virtual BOOL PreTranslateMessage(MSG* pMsg);

protected:
	// 계산기 버튼 통합 핸들러
	afx_msg void OnCalcButtonRange(UINT nID);

	// 스탑워치 버튼 핸들러들
	afx_msg void OnStwPlayStop();
	afx_msg void OnStwRecord();
	afx_msg void OnStwClear();

	// 사용자 정의 메시지(스레드 통신) 핸들러
	afx_msg LRESULT OnCalcUpdated(WPARAM, LPARAM);
	afx_msg LRESULT OnStwUpdated(WPARAM, LPARAM);
	afx_msg LRESULT OnStwLap(WPARAM, LPARAM);

private:
	Calculator m_calc; // 계산기 로직 객체
	Stopwatch  m_stw;  // 스탑워치 로직 객체

	CListCtrl m_lapList; // 랩타임 목록 리스트 컨트롤

	// 스레드 동기화 객체
	CEvent m_calcEvent; // 계산 스레드 깨우기용 알림
	CEvent m_stwEvent;  // 스탑워치 스레드 깨우기용 알림

	CCriticalSection m_calcQueueCs; // 계산 명령 큐 보호용 자물쇠
	CCriticalSection m_calcStateCs; // 계산기 상태 보호용 자물쇠
	CCriticalSection m_stwQueueCs;  // 스탑워치 명령 큐 보호용 자물쇠
	CCriticalSection m_stwStateCs;  // 스탑워치 상태 보호용 자물쇠

	// UI -> 계산기 스레드로 보낼 명령 대기열
	std::deque<Calculator::Command> m_calcQueue;

	// 스탑워치 명령 종류
	enum class SwCmdKind
	{
		ToggleStartStop,
		Lap,
		Reset
	};

	struct SwCommand
	{
		SwCmdKind kind;	 // 스톱워치 명령 종류
		LONGLONG  stamp; // 스톱워치 명령 발생 시각
	};

	// UI -> 스탑워치 스레드로 보낼 명령 대기열
	std::deque<SwCommand> m_stwQueue;

	bool m_calcExit = false; // 스레드 종료 플래그
	bool m_stwExit = false;

	CWinThread* m_calcThread = nullptr; // 계산용 작업 스레드
	CWinThread* m_stwThread = nullptr;  // 스탑워치용 작업 스레드

	// 스레드 메인 함수들 (static 필수)
	static UINT AFX_CDECL CalcThreadProc(LPVOID pParam);
	static UINT AFX_CDECL StwThreadProc(LPVOID pParam);

	// 명령 큐 삽입 헬퍼
	void EnqueueCalc(const Calculator::Command& cmd);
	void EnqueueStw(const SwCommand& cmd);
	void ApplyStwCommand(const SwCommand& cmd);

	// 화면 갱신 헬퍼
	void UpdateCalcUI();
	void UpdateStopwatchUI();

	// 랩타임 데이터 구조체
	struct LapRow
	{
		int      lapNo = 0;
		LONGLONG lapCounter = 0;
		CString  lapText;
		CString  totalText;
	};

	// 스레드간 데이터 전송용 구조체
	struct LapPayload
	{
		int      lapNo = 0;
		LONGLONG lapCounter = 0;
		CString  lapText;
		CString  totalText;
	};

	std::deque<LapRow> m_laps; // 저장된 랩타임 목록
	void RebuildLapList();     // 리스트 컨트롤 다시 그리기

	bool m_stwEverStarted = false;  // 한 번이라도 시작했는지
	bool m_stwResetPending = false; // 리셋 대기 상태 여부


	// UI 깜빡임 방지용 캐싱 변수들
	CString m_lastCalcDisp;
	CString m_lastCalcHist;
	CString m_lastNow;
	CString m_lastElapsed;
	BOOL m_lastRcEnabled = (BOOL)-1;
	BOOL m_lastClearEnabled = (BOOL)-1;
};