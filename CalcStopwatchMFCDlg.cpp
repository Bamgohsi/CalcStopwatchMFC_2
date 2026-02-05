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
	, m_calcEvent(FALSE, FALSE) // 이벤트 객체 초기화
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

	// 계산기 버튼 ID들을 하나의 함수로 매핑
	ON_COMMAND_RANGE(IDC_CAL_BTN1, IDC_CAL_BTN24, &CCalcStopwatchMFCDlg::OnCalcButtonRange)

	ON_BN_CLICKED(IDC_STWC_PS, &CCalcStopwatchMFCDlg::OnStwPlayStop)
	ON_BN_CLICKED(IDC_STWC_RC, &CCalcStopwatchMFCDlg::OnStwRecord)
	ON_BN_CLICKED(IDC_STWC_CLEAR, &CCalcStopwatchMFCDlg::OnStwClear)

	// 스레드 메시지 연결
	ON_MESSAGE(WM_APP_CALC_UPDATED, &CCalcStopwatchMFCDlg::OnCalcUpdated)
	ON_MESSAGE(WM_APP_STW_UPDATED, &CCalcStopwatchMFCDlg::OnStwUpdated)
	ON_MESSAGE(WM_APP_STW_LAP, &CCalcStopwatchMFCDlg::OnStwLap)
END_MESSAGE_MAP()


// CCalcStopwatchMFCDlg 메시지 처리기

BOOL CCalcStopwatchMFCDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 이 대화 상자의 아이콘을 설정합니다.  응용 프로그램의 주 창이 대화 상자가 아닐 경우에는
	// 프레임워크가 이 작업을 자동으로 수행합니다.
	SetIcon(m_hIcon, TRUE);			// 큰 아이콘을 설정합니다.
	SetIcon(m_hIcon, FALSE);		// 작은 아이콘을 설정합니다.

	// TODO: 여기에 추가 초기화 작업을 추가합니다.

	// 스톱워치 변수 초기화
	m_stwEverStarted = false;
	m_stwResetPending = false;
	m_laps.clear();

	// UI 캐시 초기화
	m_lastCalcDisp.Empty();
	m_lastCalcHist.Empty();
	m_lastNow.Empty();
	m_lastElapsed.Empty();
	m_lastRcEnabled = (BOOL)-1;
	m_lastClearEnabled = (BOOL)-1;

	// 리스트 컨트롤 설정 (컬럼 추가, 스타일 지정)
	m_lapList.ModifyStyle(0, LVS_REPORT);
	m_lapList.DeleteAllItems();
	while (m_lapList.DeleteColumn(0)) {}

	m_lapList.InsertColumn(0, _T("랩"), LVCFMT_LEFT, 140);
	m_lapList.InsertColumn(1, _T("시간"), LVCFMT_LEFT, 140);
	m_lapList.InsertColumn(2, _T("합계"), LVCFMT_LEFT, 140);

	DWORD ex = m_lapList.GetExtendedStyle();
	m_lapList.SetExtendedStyle(ex | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

	UpdateCalcUI(); // UI 갱신
	UpdateStopwatchUI();

	if (GetDlgItem(IDC_STWC_RC))    GetDlgItem(IDC_STWC_RC)->EnableWindow(FALSE); //기록 및 초기화 초기 비활성화
	if (GetDlgItem(IDC_STWC_CLEAR)) GetDlgItem(IDC_STWC_CLEAR)->EnableWindow(FALSE);

	// 작업 스레드 생성 및 시작
	m_calcExit = false;
	m_stwExit = false;

	m_calcThread = AfxBeginThread(&CCalcStopwatchMFCDlg::CalcThreadProc, this);
	if (m_calcThread) m_calcThread->m_bAutoDelete = FALSE; // 직접 제어하기 위해 자동 삭제 끔

	m_stwThread = AfxBeginThread(&CCalcStopwatchMFCDlg::StwThreadProc, this);
	if (m_stwThread) m_stwThread->m_bAutoDelete = FALSE;

	return TRUE;  // 포커스를 컨트롤에 설정하지 않으면 TRUE를 반환합니다.
}

// 키보드 입력 가로채기 구현
BOOL CCalcStopwatchMFCDlg::PreTranslateMessage(MSG* pMsg)
{
	// 키를 눌렀을 때
	if (pMsg->message == WM_KEYDOWN)
	{
		// 특수 키 처리
		switch (pMsg->wParam)
		{
		case VK_RETURN: // 엔터 -> '=' 버튼
			OnCalcButtonRange(IDC_CAL_BTN24);
			return TRUE; // 메시지 처리 완료 (창 닫힘 방지)

		case VK_BACK: // 백스페이스 -> '<-' 버튼
			OnCalcButtonRange(IDC_CAL_BTN4);
			return TRUE;

		case VK_ESCAPE: // ESC -> 'C' 버튼
			OnCalcButtonRange(IDC_CAL_BTN3);
			return TRUE;

		case VK_DELETE: // Delete -> 'CE' 버튼
			OnCalcButtonRange(IDC_CAL_BTN2);
			return TRUE;

		case VK_F9: // F9 -> +/- 버튼 (표준 계산기 단축키)
			OnCalcButtonRange(IDC_CAL_BTN21);
			return TRUE;
		}
	}
	// 문자가 입력될 때 (숫자, 연산자)
	else if (pMsg->message == WM_CHAR)
	{
		UINT nChar = static_cast<UINT>(pMsg->wParam);

		// 숫자 0~9 처리
		if (nChar >= '0' && nChar <= '9')
		{
			// 각 숫자에 맞는 버튼 ID 호출
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

		// 연산자 및 특수 기능 처리
		switch (nChar)
		{
		case '+': OnCalcButtonRange(IDC_CAL_BTN20); return TRUE;
		case '-': OnCalcButtonRange(IDC_CAL_BTN16); return TRUE;
		case '*':
		case 'x':
		case 'X': OnCalcButtonRange(IDC_CAL_BTN12); return TRUE; // 곱하기
		case '/': OnCalcButtonRange(IDC_CAL_BTN8);  return TRUE; // 나누기
		case '.':
		case ',': OnCalcButtonRange(IDC_CAL_BTN23); return TRUE; // 소수점
		case '=': OnCalcButtonRange(IDC_CAL_BTN24); return TRUE; // 등호
		case '%': OnCalcButtonRange(IDC_CAL_BTN1);  return TRUE; // 퍼센트
		case 'r':
		case 'R': OnCalcButtonRange(IDC_CAL_BTN5);  return TRUE; // 역수 (1/x)

		case '@': OnCalcButtonRange(IDC_CAL_BTN7);  return TRUE; // 제곱근(@)
		case 'q': OnCalcButtonRange(IDC_CAL_BTN6);  return TRUE; // 제곱(q)
		}
	}

	return CDialogEx::PreTranslateMessage(pMsg);
}

void CCalcStopwatchMFCDlg::OnDestroy()
{
	// 프로그램 종료 시 스레드 정리
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

// 계산기 UI 갱신 (스레드에서 호출됨)
void CCalcStopwatchMFCDlg::UpdateCalcUI()
{
	CString disp, hist;
	{
		CSingleLock lock(&m_calcStateCs, TRUE); // 데이터 읽는 동안 락 걸기
		disp = m_calc.GetDisplay();
		hist = m_calc.GetRSHS();
	}

	// 값이 바뀐 경우에만 갱신 (깜빡임 방지)
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

// 스탑워치 UI 갱신
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

	// 버튼 활성화/비활성화 상태 관리
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

// 계산 명령 큐 삽입
void CCalcStopwatchMFCDlg::EnqueueCalc(const Calculator::Command& cmd)
{
	{
		CSingleLock lock(&m_calcQueueCs, TRUE);
		m_calcQueue.push_back(cmd);
	}
	m_calcEvent.SetEvent(); // 스레드 깨우기
}

// 계산기 작업 스레드
UINT AFX_CDECL CCalcStopwatchMFCDlg::CalcThreadProc(LPVOID pParam)
{
	auto dlg = reinterpret_cast<CCalcStopwatchMFCDlg*>(pParam);

	while (!dlg->m_calcExit)
	{
		// 명령이 올 때까지 대기
		dlg->m_calcEvent.Lock();
		if (dlg->m_calcExit) break;

		for (;;)
		{
			Calculator::Command cmd{};
			bool hasCmd = false;

			// 큐에서 명령 꺼내기
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

			// 명령 실행
			{
				CSingleLock lock(&dlg->m_calcStateCs, TRUE);
				dlg->m_calc.Execute(cmd);
			}
		}

		// UI 갱신 요청 메시지 전송
		dlg->PostMessage(WM_APP_CALC_UPDATED, 0, 0);
	}

	return 0;
}

LRESULT CCalcStopwatchMFCDlg::OnCalcUpdated(WPARAM, LPARAM)
{
	UpdateCalcUI();
	return 0;
}

// 계산기 버튼 클릭 처리
void CCalcStopwatchMFCDlg::OnCalcButtonRange(UINT nID)
{
	const int idx = (int)nID - (int)IDC_CAL_BTN1;
	if (idx < 0 || idx >= 24) return;

	// 람다 명령 생성 코드
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

	// 버튼 ID 순서에 따른 명령 매핑 테이블
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

// 스톱워치 명령 큐 삽입
void CCalcStopwatchMFCDlg::EnqueueStw(const SwCommand& cmd)
{
	{
		CSingleLock lock(&m_stwQueueCs, TRUE);
		m_stwQueue.push_back(cmd);
	}
	m_stwEvent.SetEvent(); // 스레드 깨우기
}

// 스톱워치 명령 실행
void CCalcStopwatchMFCDlg::ApplyStwCommand(const SwCommand& cmd)
{
	switch (cmd.kind)
	{
	case SwCmdKind::ToggleStartStop:
		// 시작/중지 상태 전환
		m_stwResetPending = false;
		m_stw.ToggleStartStopAt(cmd.stamp);
		break;

	case SwCmdKind::Reset:
		// 리셋 처리
		m_stw.Reset();
		m_stwResetPending = false;
		break;

	default:
		break;
	}
}

// 스탑워치 작업 스레드
UINT AFX_CDECL CCalcStopwatchMFCDlg::StwThreadProc(LPVOID pParam)
{
	auto dlg = reinterpret_cast<CCalcStopwatchMFCDlg*>(pParam);
	CString lastNow;

	while (!dlg->m_stwExit)
	{
		// 16ms(약 60프레임)마다 깨어나거나 이벤트 발생 시 깨어남
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

					// 랩타임 발생 시 UI 스레드에 데이터 전송
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

		// 실행 중이거나 데이터 변경이 있으면 UI 갱신 요청
		if (running || processedAny || postedLap)
		{
			dlg->PostMessage(WM_APP_STW_UPDATED, 0, 0);
			continue;
		}

		// 대기 중이어도 현재 시간(HH:MM:SS) 갱신을 위해 확인
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

// 랩타임 리스트 다시 그리기
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

	// 최소/최대 시간 계산
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

	m_laps.push_front(row); // 최신 랩을 맨 위에 추가
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