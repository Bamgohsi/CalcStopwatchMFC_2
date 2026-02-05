// CalcStopwatchMFCDlg.cpp: 구현 파일
//

#include "pch.h"
#include "framework.h"
#include "CalcStopwatchMFC.h"
#include "CalcStopwatchMFCDlg.h"
#include "afxdialogex.h"
#include "resource.h"

#include <climits>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CCalcStopwatchMFCDlg 대화 상자

CCalcStopwatchMFCDlg::CCalcStopwatchMFCDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_CALCSTOPWATCHMFC_DIALOG, pParent)
	, m_calcEvent(FALSE, FALSE)
	, m_stwEvent(FALSE, FALSE)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CCalcStopwatchMFCDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_STWC, m_lapList);
}

BEGIN_MESSAGE_MAP(CCalcStopwatchMFCDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_DESTROY()

	ON_COMMAND_RANGE(IDC_CAL_BTN1, IDC_CAL_BTN24, &CCalcStopwatchMFCDlg::OnCalcButtonRange)

	ON_BN_CLICKED(IDC_STWC_PS, &CCalcStopwatchMFCDlg::OnStwPlayStop)
	ON_BN_CLICKED(IDC_STWC_RC, &CCalcStopwatchMFCDlg::OnStwRecord)
	ON_BN_CLICKED(IDC_STWC_CLEAR, &CCalcStopwatchMFCDlg::OnStwClear)

	ON_MESSAGE(WM_APP_CALC_UPDATED, &CCalcStopwatchMFCDlg::OnCalcUpdated)
	ON_MESSAGE(WM_APP_STW_UPDATED, &CCalcStopwatchMFCDlg::OnStwUpdated)
	ON_MESSAGE(WM_APP_STW_LAP, &CCalcStopwatchMFCDlg::OnStwLap)
END_MESSAGE_MAP()


// CCalcStopwatchMFCDlg 메시지 처리기

BOOL CCalcStopwatchMFCDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 이 대화 상자의 아이콘을 설정합니다.  응용 프로그램의 주 창이 대화 상자가 아닐 경우에는
	//  프레임워크가 이 작업을 자동으로 수행합니다.
	SetIcon(m_hIcon, TRUE);			// 큰 아이콘을 설정합니다.
	SetIcon(m_hIcon, FALSE);		// 작은 아이콘을 설정합니다.

	// TODO: 여기에 추가 초기화 작업을 추가합니다.

	m_stwEverStarted = false;
	m_stwResetPending = false;
	m_laps.clear();

	m_lastCalcDisp.Empty();
	m_lastCalcHist.Empty();
	m_lastNow.Empty();
	m_lastElapsed.Empty();
	m_lastRcEnabled = (BOOL)-1;
	m_lastClearEnabled = (BOOL)-1;

	m_lapList.ModifyStyle(0, LVS_REPORT);
	m_lapList.DeleteAllItems();
	while (m_lapList.DeleteColumn(0)) {}

	m_lapList.InsertColumn(0, _T("랩"), LVCFMT_LEFT, 140);
	m_lapList.InsertColumn(1, _T("시간"), LVCFMT_LEFT, 140);
	m_lapList.InsertColumn(2, _T("합계"), LVCFMT_LEFT, 140);

	DWORD ex = m_lapList.GetExtendedStyle();
	m_lapList.SetExtendedStyle(ex | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

	UpdateCalcUI();
	UpdateStopwatchUI();

	if (GetDlgItem(IDC_STWC_RC))    GetDlgItem(IDC_STWC_RC)->EnableWindow(FALSE);
	if (GetDlgItem(IDC_STWC_CLEAR)) GetDlgItem(IDC_STWC_CLEAR)->EnableWindow(FALSE);

	m_calcExit = false;
	m_stwExit = false;

	m_calcThread = AfxBeginThread(&CCalcStopwatchMFCDlg::CalcThreadProc, this);
	if (m_calcThread) m_calcThread->m_bAutoDelete = FALSE;

	m_stwThread = AfxBeginThread(&CCalcStopwatchMFCDlg::StwThreadProc, this);
	if (m_stwThread) m_stwThread->m_bAutoDelete = FALSE;

	return TRUE;  // 포커스를 컨트롤에 설정하지 않으면 TRUE를 반환합니다.
}

// ge: 메시지 전처리 함수 구현입니다.
// ge: 윈도우 메시지가 처리되기 전에 키보드 입력을 가로채서 계산기 버튼 동작으로 변환합니다.
BOOL CCalcStopwatchMFCDlg::PreTranslateMessage(MSG* pMsg)
{
	// ge: 키보드를 눌렀을 때의 메시지인지 확인합니다.
	if (pMsg->message == WM_KEYDOWN)
	{
		// ge: 특수 키(엔터, 백스페이스, ESC 등) 처리를 위한 switch문입니다.
		switch (pMsg->wParam)
		{
		case VK_RETURN:
			// ge: 엔터키는 '=' 버튼(IDC_CAL_BTN24)과 매핑합니다. 
			// ge: 기본 동작(대화상자 닫힘)을 막기 위해 버튼 함수를 직접 호출하고 TRUE를 반환합니다.
			OnCalcButtonRange(IDC_CAL_BTN24);
			return TRUE;

		case VK_BACK:
			// ge: 백스페이스키는 '⌫' 버튼(IDC_CAL_BTN4)과 매핑합니다.
			OnCalcButtonRange(IDC_CAL_BTN4);
			return TRUE;

		case VK_ESCAPE:
			// ge: ESC키는 'C' 버튼(IDC_CAL_BTN3, 전체 초기화)과 매핑합니다.
			// ge: 대화상자가 닫히는 것을 방지합니다.
			OnCalcButtonRange(IDC_CAL_BTN3);
			return TRUE;

		case VK_DELETE:
			// ge: Delete키는 'CE' 버튼(IDC_CAL_BTN2, 입력 초기화)과 매핑합니다.
			OnCalcButtonRange(IDC_CAL_BTN2);
			return TRUE;

		case VK_F9:
			// ge: F9키는 표준 계산기처럼 '±' 버튼(IDC_CAL_BTN21, 부호 반전)과 매핑합니다.
			OnCalcButtonRange(IDC_CAL_BTN21);
			return TRUE;
		}
	}
	// ge: 문자가 입력되었을 때의 메시지인지 확인합니다.
	else if (pMsg->message == WM_CHAR)
	{
		UINT nChar = static_cast<UINT>(pMsg->wParam);

		// ge: 숫자 0~9 처리
		if (nChar >= '0' && nChar <= '9')
		{
			// ge: 0~9 숫자에 해당하는 버튼 ID를 계산하여 호출합니다.
			switch (nChar)
			{
			case '0': OnCalcButtonRange(IDC_CAL_BTN22); break;
			case '1': OnCalcButtonRange(IDC_CAL_BTN17); break;
			case '2': OnCalcButtonRange(IDC_CAL_BTN18); break;
			case '3': OnCalcButtonRange(IDC_CAL_BTN19); break;
			case '4': OnCalcButtonRange(IDC_CAL_BTN13); break;
			case '5': OnCalcButtonRange(IDC_CAL_BTN14); break;
			case '6': OnCalcButtonRange(IDC_CAL_BTN15); break;
			case '7': OnCalcButtonRange(IDC_CAL_BTN9); break;
			case '8': OnCalcButtonRange(IDC_CAL_BTN10); break;
			case '9': OnCalcButtonRange(IDC_CAL_BTN11); break;
			}
			return TRUE; // ge: 메시지를 처리했음을 알립니다.
		}

		// ge: 연산자 및 기타 기호 처리
		switch (nChar)
		{
		case '+':
			OnCalcButtonRange(IDC_CAL_BTN20); // + 버튼
			return TRUE;
		case '-':
			OnCalcButtonRange(IDC_CAL_BTN16); // - 버튼
			return TRUE;
		case '*':
		case 'x':
		case 'X':
			OnCalcButtonRange(IDC_CAL_BTN12); // × 버튼
			return TRUE;
		case '/':
			OnCalcButtonRange(IDC_CAL_BTN8);  // ÷ 버튼
			return TRUE;
		case '.':
		case ',':
			OnCalcButtonRange(IDC_CAL_BTN23); // . 버튼 (소수점)
			return TRUE;
		case '=':
			OnCalcButtonRange(IDC_CAL_BTN24); // = 버튼
			return TRUE;
		case '%':
			OnCalcButtonRange(IDC_CAL_BTN1);  // % 버튼
			return TRUE;
		case 'r':
		case 'R':
			OnCalcButtonRange(IDC_CAL_BTN5);  // 1/x 버튼 (Reciprocal)
			return TRUE;

		case '@':
			// ge: 요청하신 대로 제곱근은 오직 '@' 키로만 동작합니다.
			OnCalcButtonRange(IDC_CAL_BTN7);  // √x 버튼 (Sqrt)
			return TRUE;

		case 'q':
			// ge: 요청하신 대로 제곱은 오직 'q' 키로만 동작합니다.
			OnCalcButtonRange(IDC_CAL_BTN6);  // x² 버튼 (Square)
			return TRUE;
		}
	}

	return CDialogEx::PreTranslateMessage(pMsg);
}

void CCalcStopwatchMFCDlg::OnDestroy()
{
	m_calcExit = true;
	m_stwExit = true;

	m_calcEvent.SetEvent();
	m_stwEvent.SetEvent();

	if (m_calcThread)
	{
		::WaitForSingleObject(m_calcThread->m_hThread, INFINITE);
		delete m_calcThread;
		m_calcThread = nullptr;
	}

	if (m_stwThread)
	{
		::WaitForSingleObject(m_stwThread->m_hThread, INFINITE);
		delete m_stwThread;
		m_stwThread = nullptr;
	}

	CDialogEx::OnDestroy();
}

// 대화 상자에 최소화 단추를 추가할 경우 아이콘을 그리려면
//  아래 코드가 필요합니다.  문서/뷰 모델을 사용하는 MFC 애플리케이션의 경우에는
//  프레임워크에서 이 작업을 자동으로 수행합니다.

void CCalcStopwatchMFCDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 그리기를 위한 디바이스 컨텍스트입니다.

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 클라이언트 사각형에서 아이콘을 가운데에 맞춥니다.
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 아이콘을 그립니다.
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// 사용자가 최소화된 창을 끄는 동안에 커서가 표시되도록 시스템에서
//  이 함수를 호출합니다.
HCURSOR CCalcStopwatchMFCDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CCalcStopwatchMFCDlg::UpdateCalcUI()
{
	CString disp, hist;
	{
		CSingleLock lock(&m_calcStateCs, TRUE);
		disp = m_calc.GetDisplay();
		hist = m_calc.GetRSHS();
	}

	if (disp != m_lastCalcDisp)
	{
		SetDlgItemText(IDC_CAL_RESULT, disp);
		m_lastCalcDisp = disp;
	}

	if (hist != m_lastCalcHist)
	{
		SetDlgItemText(IDC_CAL_RS_HS, hist);
		m_lastCalcHist = hist;
	}
}

void CCalcStopwatchMFCDlg::UpdateStopwatchUI()
{
	CString now;
	CString elapsed;
	bool running = false;
	bool resetPending = false;

	now = m_stw.GetNowText();

	{
		CSingleLock lock(&m_stwStateCs, TRUE);
		running = m_stw.IsRunning();
		resetPending = m_stwResetPending;
		elapsed = resetPending ? _T("00:00:00.00") : m_stw.GetElapsedText();
	}

	if (now != m_lastNow)
	{
		SetDlgItemText(IDC_TIME, now);
		m_lastNow = now;
	}

	if (elapsed != m_lastElapsed)
	{
		SetDlgItemText(IDC_STWC_RESULT, elapsed);
		m_lastElapsed = elapsed;
	}

	const bool dirty = (!m_laps.empty() || m_stwEverStarted);

	const BOOL rcEnabled = running ? TRUE : FALSE;
	const BOOL clearEnabled = dirty ? TRUE : FALSE;

	if (m_lastRcEnabled != rcEnabled)
	{
		if (GetDlgItem(IDC_STWC_RC))
			GetDlgItem(IDC_STWC_RC)->EnableWindow(rcEnabled);
		m_lastRcEnabled = rcEnabled;
	}

	if (m_lastClearEnabled != clearEnabled)
	{
		if (GetDlgItem(IDC_STWC_CLEAR))
			GetDlgItem(IDC_STWC_CLEAR)->EnableWindow(clearEnabled);
		m_lastClearEnabled = clearEnabled;
	}
}

void CCalcStopwatchMFCDlg::EnqueueCalc(const Calculator::Command& cmd)
{
	{
		CSingleLock lock(&m_calcQueueCs, TRUE);
		m_calcQueue.push_back(cmd);
	}
	m_calcEvent.SetEvent();
}

UINT AFX_CDECL CCalcStopwatchMFCDlg::CalcThreadProc(LPVOID pParam)
{
	auto dlg = reinterpret_cast<CCalcStopwatchMFCDlg*>(pParam);

	while (!dlg->m_calcExit)
	{
		dlg->m_calcEvent.Lock();
		if (dlg->m_calcExit) break;

		for (;;)
		{
			Calculator::Command cmd{};
			bool hasCmd = false;

			{
				CSingleLock lock(&dlg->m_calcQueueCs, TRUE);
				if (!dlg->m_calcQueue.empty())
				{
					cmd = dlg->m_calcQueue.front();
					dlg->m_calcQueue.pop_front();
					hasCmd = true;
				}
			}

			if (!hasCmd) break;

			{
				CSingleLock lock(&dlg->m_calcStateCs, TRUE);
				dlg->m_calc.Execute(cmd);
			}
		}

		dlg->PostMessage(WM_APP_CALC_UPDATED, 0, 0);
	}

	return 0;
}

LRESULT CCalcStopwatchMFCDlg::OnCalcUpdated(WPARAM, LPARAM)
{
	UpdateCalcUI();
	return 0;
}

void CCalcStopwatchMFCDlg::OnCalcButtonRange(UINT nID)
{
	const int idx = (int)nID - (int)IDC_CAL_BTN1;
	if (idx < 0 || idx >= 24) return;

	auto MakeDigit = [](int d) {
		Calculator::Command c{};
		c.kind = Calculator::CmdKind::Digit;
		c.digit = d;
		return c;
	};
	auto MakeOp = [](Calculator::Op op) {
		Calculator::Command c{};
		c.kind = Calculator::CmdKind::Op;
		c.op = op;
		return c;
	};
	auto MakeCmd = [](Calculator::CmdKind k) {
		Calculator::Command c{};
		c.kind = k;
		return c;
	};

	static const Calculator::Command kMap[24] = {
		/* 0  %   */ MakeCmd(Calculator::CmdKind::Percent),
		/* 1  CE  */ MakeCmd(Calculator::CmdKind::ClearEntry),
		/* 2  C   */ MakeCmd(Calculator::CmdKind::Clear),
		/* 3  BS  */ MakeCmd(Calculator::CmdKind::Backspace),
		/* 4  1/x */ MakeCmd(Calculator::CmdKind::Reciprocal),
		/* 5  x^2 */ MakeCmd(Calculator::CmdKind::Square),
		/* 6  sqrt*/ MakeCmd(Calculator::CmdKind::SqrtX),
		/* 7  ÷   */ MakeOp(Calculator::Op::Div),

		/* 8  7   */ MakeDigit(7),
		/* 9  8   */ MakeDigit(8),
		/* 10 9   */ MakeDigit(9),
		/* 11 ×   */ MakeOp(Calculator::Op::Mul),

		/* 12 4   */ MakeDigit(4),
		/* 13 5   */ MakeDigit(5),
		/* 14 6   */ MakeDigit(6),
		/* 15 -   */ MakeOp(Calculator::Op::Sub),

		/* 16 1   */ MakeDigit(1),
		/* 17 2   */ MakeDigit(2),
		/* 18 3   */ MakeDigit(3),
		/* 19 +   */ MakeOp(Calculator::Op::Add),

		/* 20 ±   */ MakeCmd(Calculator::CmdKind::ToggleSign),
		/* 21 0   */ MakeDigit(0),
		/* 22 .   */ MakeCmd(Calculator::CmdKind::Decimal),
		/* 23 =   */ MakeCmd(Calculator::CmdKind::Equal),
	};

	EnqueueCalc(kMap[idx]);
}

void CCalcStopwatchMFCDlg::EnqueueStw(const SwCommand& cmd)
{
	{
		CSingleLock lock(&m_stwQueueCs, TRUE);
		m_stwQueue.push_back(cmd);
	}
	m_stwEvent.SetEvent();
}

void CCalcStopwatchMFCDlg::ApplyStwCommand(const SwCommand& cmd)
{
	switch (cmd.kind)
	{
	case SwCmdKind::ToggleStartStop:
		m_stwResetPending = false;
		m_stw.ToggleStartStopAt(cmd.stamp);
		break;
	case SwCmdKind::Reset:
		m_stw.Reset();
		m_stwResetPending = false;
		break;
	default:
		break;
	}
}

UINT AFX_CDECL CCalcStopwatchMFCDlg::StwThreadProc(LPVOID pParam)
{
	auto dlg = reinterpret_cast<CCalcStopwatchMFCDlg*>(pParam);
	CString lastNow;

	while (!dlg->m_stwExit)
	{
		BOOL signaled = dlg->m_stwEvent.Lock(16);
		if (dlg->m_stwExit) break;

		bool processedAny = false;
		bool postedLap = false;

		if (signaled)
		{
			for (;;)
			{
				SwCommand cmd{};
				bool hasCmd = false;

				{
					CSingleLock lock(&dlg->m_stwQueueCs, TRUE);
					if (!dlg->m_stwQueue.empty())
					{
						cmd = dlg->m_stwQueue.front();
						dlg->m_stwQueue.pop_front();
						hasCmd = true;
					}
				}

				if (!hasCmd) break;
				processedAny = true;

				if (cmd.kind == SwCmdKind::Lap)
				{
					Stopwatch::LapSnapshot snap;
					{
						CSingleLock lock(&dlg->m_stwStateCs, TRUE);
						snap = dlg->m_stw.LapAt(cmd.stamp);
					}

					if (snap.lapNo > 0)
					{
						auto payload = new LapPayload;
						payload->lapNo = snap.lapNo;
						payload->lapCounter = snap.lapCounter;

						{
							CSingleLock lock(&dlg->m_stwStateCs, TRUE);
							payload->lapText = dlg->m_stw.FormatCounter(snap.lapCounter);
							payload->totalText = dlg->m_stw.FormatCounter(snap.totalCounter);
						}

						dlg->PostMessage(WM_APP_STW_LAP, (WPARAM)payload, 0);
						postedLap = true;
					}
				}
				else
				{
					CSingleLock lock(&dlg->m_stwStateCs, TRUE);
					dlg->ApplyStwCommand(cmd);
				}
			}
		}

		bool running = false;
		{
			CSingleLock lock(&dlg->m_stwStateCs, TRUE);
			running = dlg->m_stw.IsRunning();
		}

		if (running || processedAny || postedLap)
		{
			dlg->PostMessage(WM_APP_STW_UPDATED, 0, 0);
			continue;
		}

		CString now = dlg->m_stw.GetNowText();
		if (now != lastNow)
		{
			lastNow = now;
			dlg->PostMessage(WM_APP_STW_UPDATED, 0, 0);
		}
	}

	return 0;
}

LRESULT CCalcStopwatchMFCDlg::OnStwUpdated(WPARAM, LPARAM)
{
	UpdateStopwatchUI();
	return 0;
}

void CCalcStopwatchMFCDlg::RebuildLapList()
{
	m_lapList.SetRedraw(FALSE);
	m_lapList.DeleteAllItems();

	const int n = (int)m_laps.size();
	if (n <= 0)
	{
		m_lapList.SetRedraw(TRUE);
		m_lapList.Invalidate();
		return;
	}

	int minIdx = -1, maxIdx = -1;

	if (n >= 2)
	{
		LONGLONG minV = LLONG_MAX;
		LONGLONG maxV = LLONG_MIN;

		for (int i = 0; i < n; i++)
		{
			const LONGLONG v = m_laps[i].lapCounter;
			if (v < minV) { minV = v; minIdx = i; }
			if (v > maxV) { maxV = v; maxIdx = i; }
		}

		if (minV == maxV) { minIdx = -1; maxIdx = -1; }
	}

	for (int i = 0; i < n; i++)
	{
		CString col0;
		col0.Format(_T("%d"), m_laps[i].lapNo);
		if (i == minIdx) col0 += _T("  가장 빠름");
		else if (i == maxIdx) col0 += _T("  가장 느림");

		int row = m_lapList.InsertItem(i, col0);
		m_lapList.SetItemText(row, 1, m_laps[i].lapText);
		m_lapList.SetItemText(row, 2, m_laps[i].totalText);
	}

	m_lapList.SetRedraw(TRUE);
	m_lapList.Invalidate();
}

LRESULT CCalcStopwatchMFCDlg::OnStwLap(WPARAM wParam, LPARAM)
{
	auto payload = reinterpret_cast<LapPayload*>(wParam);
	if (!payload) return 0;

	LapRow row{};
	row.lapNo = payload->lapNo;
	row.lapCounter = payload->lapCounter;
	row.lapText = payload->lapText;
	row.totalText = payload->totalText;

	m_laps.push_front(row);
	RebuildLapList();

	delete payload;
	return 0;
}

void CCalcStopwatchMFCDlg::OnStwPlayStop()
{
	bool running = false;
	{
		CSingleLock lock(&m_stwStateCs, TRUE);
		running = m_stw.IsRunning();
		m_stwResetPending = false;
	}

	if (!running) m_stwEverStarted = true;

	LARGE_INTEGER c;
	::QueryPerformanceCounter(&c);
	EnqueueStw(SwCommand{ SwCmdKind::ToggleStartStop, c.QuadPart });
}

void CCalcStopwatchMFCDlg::OnStwRecord()
{
	bool running = false;
	{
		CSingleLock lock(&m_stwStateCs, TRUE);
		running = m_stw.IsRunning();
	}
	if (!running) return;

	LARGE_INTEGER c;
	::QueryPerformanceCounter(&c);
	EnqueueStw(SwCommand{ SwCmdKind::Lap, c.QuadPart });
}

void CCalcStopwatchMFCDlg::OnStwClear()
{
	bool running = false;
	{
		CSingleLock lock(&m_stwStateCs, TRUE);
		running = m_stw.IsRunning();
		m_stwResetPending = true;
	}

	if (running)
	{
		LARGE_INTEGER c;
		::QueryPerformanceCounter(&c);
		EnqueueStw(SwCommand{ SwCmdKind::ToggleStartStop, c.QuadPart });
	}

	EnqueueStw(SwCommand{ SwCmdKind::Reset, 0 });

	m_laps.clear();
	m_lapList.DeleteAllItems();
	m_stwEverStarted = false;

	if (GetDlgItem(IDC_STWC_RC))    GetDlgItem(IDC_STWC_RC)->EnableWindow(FALSE);
	if (GetDlgItem(IDC_STWC_CLEAR)) GetDlgItem(IDC_STWC_CLEAR)->EnableWindow(FALSE);

	SetDlgItemText(IDC_STWC_RESULT, _T("00:00:00.00"));
}