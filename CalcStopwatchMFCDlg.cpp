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

CCalcStopwatchMFCDlg::CCalcStopwatchMFCDlg(CWnd* pParent)
	: CDialogEx(IDD_CALCSTOPWATCHMFC_DIALOG, pParent)
	, m_calcWorker(this)
	, m_stwWorker(this)
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

	// 계산기 버튼 통합 처리
	ON_COMMAND_RANGE(IDC_CAL_BTN1, IDC_CAL_BTN24, &CCalcStopwatchMFCDlg::OnCalcButtonRange)

	ON_BN_CLICKED(IDC_STWC_PS, &CCalcStopwatchMFCDlg::OnStwPlayStop)
	ON_BN_CLICKED(IDC_STWC_RC, &CCalcStopwatchMFCDlg::OnStwRecord)
	ON_BN_CLICKED(IDC_STWC_CLEAR, &CCalcStopwatchMFCDlg::OnStwClear)

	// 워커 스레드 -> UI 갱신
	ON_MESSAGE(WM_APP_CALC_UPDATED, &CCalcStopwatchMFCDlg::OnCalcUpdated)
	ON_MESSAGE(WM_APP_STW_UPDATED, &CCalcStopwatchMFCDlg::OnStwUpdated)
	ON_MESSAGE(WM_APP_STW_LAP, &CCalcStopwatchMFCDlg::OnStwLap)
END_MESSAGE_MAP()


// CCalcStopwatchMFCDlg 메시지 처리기

BOOL CCalcStopwatchMFCDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 초기 UI 상태/캐시 값 셋업
	SetIcon(m_hIcon, TRUE);
	SetIcon(m_hIcon, FALSE);

	m_stwEverStarted = false;
	m_laps.clear();

	m_lastCalcDisp.Empty();
	m_lastCalcHist.Empty();
	m_lastNow.Empty();
	m_lastElapsed.Empty();
	m_lastRcEnabled = (BOOL)-1;
	m_lastClearEnabled = (BOOL)-1;

	// 랩 리스트 초기화 (열 구성/스타일)
	m_lapList.ModifyStyle(0, LVS_REPORT);
	m_lapList.DeleteAllItems();
	while (m_lapList.DeleteColumn(0)) {}

	m_lapList.InsertColumn(0, _T("랩"), LVCFMT_LEFT, 140);
	m_lapList.InsertColumn(1, _T("시간"), LVCFMT_LEFT, 140);
	m_lapList.InsertColumn(2, _T("합계"), LVCFMT_LEFT, 140);

	DWORD ex = m_lapList.GetExtendedStyle();
	m_lapList.SetExtendedStyle(ex | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

	// 초기 표시 갱신
	UpdateCalcUI();
	UpdateStopwatchUI();

	if (GetDlgItem(IDC_STWC_RC))    GetDlgItem(IDC_STWC_RC)->EnableWindow(FALSE);
	if (GetDlgItem(IDC_STWC_CLEAR)) GetDlgItem(IDC_STWC_CLEAR)->EnableWindow(FALSE);

	// 현재값 입력창 폰트 크게 (24pt)
	m_calcResultFont.CreatePointFont(240, _T("Segoe UI"));
	if (auto p = GetDlgItem(IDC_CAL_RESULT))
		p->SetFont(&m_calcResultFont);

	// 스톱워치 경과 시간 표시 폰트 크게 (40pt)
	m_stwResultFont.CreatePointFont(400, _T("Segoe UI"));
	if (auto p = GetDlgItem(IDC_STWC_RESULT))
		p->SetFont(&m_stwResultFont);

	// 스톱워치 라벨 폰트 (13pt)
	m_stwLabelFont.CreatePointFont(130, _T("Segoe UI"));
	if (auto p = GetDlgItem(IDC_STWC_H)) p->SetFont(&m_stwLabelFont);
	if (auto p = GetDlgItem(IDC_STWC_M)) p->SetFont(&m_stwLabelFont);
	if (auto p = GetDlgItem(IDC_STWC_S)) p->SetFont(&m_stwLabelFont);

	// 워커 스레드 시작 (계산기/스톱워치)
	m_calcWorker.Start();
	m_stwWorker.Start();

	return TRUE;
}

// 키보드 입력 매핑
BOOL CCalcStopwatchMFCDlg::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN)
	{
		// 특수키 -> 계산기 버튼 매핑
		switch (pMsg->wParam)
		{
		case VK_RETURN: // 엔터 -> '='
			OnCalcButtonRange(IDC_CAL_BTN24);
			return TRUE;

		case VK_BACK: // 백스페이스 -> '<-'
			OnCalcButtonRange(IDC_CAL_BTN4);
			return TRUE;

		case VK_ESCAPE: // ESC -> 'C'
			OnCalcButtonRange(IDC_CAL_BTN3);
			return TRUE;

		case VK_DELETE: // Delete -> 'CE'
			OnCalcButtonRange(IDC_CAL_BTN2);
			return TRUE;

		case VK_F9: // F9 -> '+/-'
			OnCalcButtonRange(IDC_CAL_BTN21);
			return TRUE;
		}
	}
	else if (pMsg->message == WM_CHAR)
	{
		// 문자 입력 -> 계산기 버튼 매핑
		UINT nChar = static_cast<UINT>(pMsg->wParam);

		if (nChar >= '0' && nChar <= '9')
		{
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
			return TRUE;
		}

		switch (nChar)
		{
		case '+': OnCalcButtonRange(IDC_CAL_BTN20); return TRUE;
		case '-': OnCalcButtonRange(IDC_CAL_BTN16); return TRUE;
		case '*':
		case 'x':
		case 'X': OnCalcButtonRange(IDC_CAL_BTN12); return TRUE;
		case '/': OnCalcButtonRange(IDC_CAL_BTN8);  return TRUE;
		case '.':
		case ',': OnCalcButtonRange(IDC_CAL_BTN23); return TRUE;
		case '=': OnCalcButtonRange(IDC_CAL_BTN24); return TRUE;
		case '%': OnCalcButtonRange(IDC_CAL_BTN1);  return TRUE;
		case 'r':
		case 'R': OnCalcButtonRange(IDC_CAL_BTN5);  return TRUE;

		case '@': OnCalcButtonRange(IDC_CAL_BTN7);  return TRUE;
		case 'q': OnCalcButtonRange(IDC_CAL_BTN6);  return TRUE;
		}
	}

	return CDialogEx::PreTranslateMessage(pMsg);
}

void CCalcStopwatchMFCDlg::OnDestroy()
{
	m_calcWorker.Stop();
	m_stwWorker.Stop();

	CDialogEx::OnDestroy();
}

void CCalcStopwatchMFCDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this);

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

HCURSOR CCalcStopwatchMFCDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

// 계산기 UI 갱신
void CCalcStopwatchMFCDlg::UpdateCalcUI()
{
	// 계산기 상태 읽기 (스레드 세이프)
	CString disp = m_calcWorker.GetDisplay();
	CString hist = m_calcWorker.GetHistory();

	if (disp != m_lastCalcDisp)
	{
		// 결과창 갱신
		SetDlgItemText(IDC_CAL_RESULT, disp);
		m_lastCalcDisp = disp;
	}

	if (hist != m_lastCalcHist)
	{
		// 히스토리(상단식) 갱신
		SetDlgItemText(IDC_CAL_RS_HS, hist);
		m_lastCalcHist = hist;
	}
}

// 스톱워치 UI 갱신
void CCalcStopwatchMFCDlg::UpdateStopwatchUI()
{
	CString now;
	CString elapsed;
	bool running = false;
	bool resetPending = false;

	// 현재 시각은 스레드 동기화 없이 조회
	now = m_stwWorker.GetNowText();

	// 경과 시간/상태는 동기화 후 조회
	running = m_stwWorker.IsRunning();
	resetPending = m_stwWorker.GetResetPending();
	elapsed = resetPending ? _T("00:00:00.00") : m_stwWorker.GetElapsedText();

	if (now != m_lastNow)
	{
		// 상단 현재 시각 갱신
		SetDlgItemText(IDC_TIME, now);
		m_lastNow = now;
	}

	if (elapsed != m_lastElapsed)
	{
		// 스톱워치 경과 시간 갱신
		SetDlgItemText(IDC_STWC_RESULT, elapsed);
		m_lastElapsed = elapsed;
	}

	const bool dirty = (!m_laps.empty() || m_stwEverStarted);

	const BOOL rcEnabled = running ? TRUE : FALSE;
	const BOOL clearEnabled = dirty ? TRUE : FALSE;

	if (m_lastRcEnabled != rcEnabled)
	{
		// 기록 버튼 활성/비활성
		if (GetDlgItem(IDC_STWC_RC))
			GetDlgItem(IDC_STWC_RC)->EnableWindow(rcEnabled);
		m_lastRcEnabled = rcEnabled;
	}

	if (m_lastClearEnabled != clearEnabled)
	{
		// 초기화 버튼 활성/비활성
		if (GetDlgItem(IDC_STWC_CLEAR))
			GetDlgItem(IDC_STWC_CLEAR)->EnableWindow(clearEnabled);
		m_lastClearEnabled = clearEnabled;
	}
}

// 계산기 명령 큐
void CCalcStopwatchMFCDlg::EnqueueCalc(const Calculator::Command& cmd)
{
	m_calcWorker.Enqueue(cmd);
}

LRESULT CCalcStopwatchMFCDlg::OnCalcUpdated(WPARAM, LPARAM)
{
	UpdateCalcUI();
	return 0;
}

void CCalcStopwatchMFCDlg::OnCalcButtonRange(UINT nID)
{
	// 버튼 ID -> 명령 매핑
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
		MakeCmd(Calculator::CmdKind::Percent),		// 0번  (%)
		MakeCmd(Calculator::CmdKind::ClearEntry),	// 1번  (CE)
		MakeCmd(Calculator::CmdKind::Clear),		// 2번  (C)
		MakeCmd(Calculator::CmdKind::Backspace),	// 3번  (BS)
		MakeCmd(Calculator::CmdKind::Reciprocal),	// 4번  (1/x)
		MakeCmd(Calculator::CmdKind::Square),		// 5번  (x^2)
		MakeCmd(Calculator::CmdKind::SqrtX),		// 6번  (sqrt)
		MakeOp(Calculator::Op::Div),				// 7번  (÷)

		MakeDigit(7),								// 8번  (7)
		MakeDigit(8),								// 9번  (8)
		MakeDigit(9),								// 10번 (9)
		MakeOp(Calculator::Op::Mul),				// 11번 (×)

		MakeDigit(4),								// 12번 (4)
		MakeDigit(5),								// 13번 (5)
		MakeDigit(6),								// 14번 (6)
		MakeOp(Calculator::Op::Sub),				// 15번 (-)

		MakeDigit(1),								// 16번 (1)
		MakeDigit(2),								// 17번 (2)
		MakeDigit(3),								// 18번 (3)
		MakeOp(Calculator::Op::Add),				// 19번 (+)

		MakeCmd(Calculator::CmdKind::ToggleSign),	// 20번 (±)
		MakeDigit(0),								// 21번 (0)
		MakeCmd(Calculator::CmdKind::Decimal),		// 22번 (.)
		MakeCmd(Calculator::CmdKind::Equal),		// 23번 (=)
	};

	EnqueueCalc(kMap[idx]);
}

// 스톱워치 명령 큐
void CCalcStopwatchMFCDlg::EnqueueStw(const StopwatchWorker::Command& cmd)
{
	m_stwWorker.Enqueue(cmd);
}

LRESULT CCalcStopwatchMFCDlg::OnStwUpdated(WPARAM, LPARAM)
{
	UpdateStopwatchUI();
	return 0;
}

// 랩 리스트 재구성
void CCalcStopwatchMFCDlg::RebuildLapList()
{
	// 랩 리스트 전체 재구성 (최신이 상단)
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
		// 빠름/느림 표시
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
	// 스레드에서 전달된 랩 정보 반영
	auto payload = reinterpret_cast<StopwatchWorker::LapPayload*>(wParam);
	if (!payload) return 0;

	LapRow row{};
	row.lapNo = payload->lapNo;
	row.lapCounter = payload->lapCounter;
	row.lapText = payload->lapText;
	row.totalText = payload->totalText;

	m_laps.push_front(row); // 최신 랩을 맨 위에
	RebuildLapList();

	delete payload;
	return 0;
}

void CCalcStopwatchMFCDlg::OnStwPlayStop()
{
	// 시작/정지 토글
	bool running = m_stwWorker.IsRunning();
	m_stwWorker.SetResetPending(false);

	if (!running) m_stwEverStarted = true;

	LARGE_INTEGER c;
	::QueryPerformanceCounter(&c);
	EnqueueStw(StopwatchWorker::Command{ StopwatchWorker::CommandKind::ToggleStartStop, c.QuadPart });
}

void CCalcStopwatchMFCDlg::OnStwRecord()
{
	// 랩 기록
	bool running = m_stwWorker.IsRunning();
	if (!running) return;

	LARGE_INTEGER c;
	::QueryPerformanceCounter(&c);
	EnqueueStw(StopwatchWorker::Command{ StopwatchWorker::CommandKind::Lap, c.QuadPart });
}

void CCalcStopwatchMFCDlg::OnStwClear()
{
	// 초기화 (동작 중이면 정지 후 리셋)
	bool running = m_stwWorker.IsRunning();
	m_stwWorker.SetResetPending(true);

	if (running)
	{
		LARGE_INTEGER c;
		::QueryPerformanceCounter(&c);
		EnqueueStw(StopwatchWorker::Command{ StopwatchWorker::CommandKind::ToggleStartStop, c.QuadPart });
	}

	EnqueueStw(StopwatchWorker::Command{ StopwatchWorker::CommandKind::Reset, 0 });

	m_laps.clear();
	m_lapList.DeleteAllItems();
	m_stwEverStarted = false;

	if (GetDlgItem(IDC_STWC_RC))    GetDlgItem(IDC_STWC_RC)->EnableWindow(FALSE);
	if (GetDlgItem(IDC_STWC_CLEAR)) GetDlgItem(IDC_STWC_CLEAR)->EnableWindow(FALSE);

	SetDlgItemText(IDC_STWC_RESULT, _T("00:00:00.00"));
}
