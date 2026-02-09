// CalcStopwatchMFCDlg.h: 헤더 파일
//

#pragma once

#include "CalculatorWorker.h"
#include "StopwatchWorker.h"
#include "AppMessages.h"

#include <deque>


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
	CalculatorWorker m_calcWorker;
	StopwatchWorker  m_stwWorker;

	CListCtrl m_lapList;
	CFont m_calcResultFont;
	CFont m_stwResultFont;
	CFont m_stwLabelFont;

	// 명령 큐 삽입
	void EnqueueCalc(const Calculator::Command& cmd);
	void EnqueueStw(const StopwatchWorker::Command& cmd);

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

	std::deque<LapRow> m_laps;
	void RebuildLapList();

	bool m_stwEverStarted = false;

	// UI 캐시
	CString m_lastCalcDisp;
	CString m_lastCalcHist;
	CString m_lastNow;
	CString m_lastElapsed;
	BOOL m_lastRcEnabled = (BOOL)-1;
	BOOL m_lastClearEnabled = (BOOL)-1;
};
