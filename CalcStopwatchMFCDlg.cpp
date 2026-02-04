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

CCalcStopwatchMFCDlg::CCalcStopwatchMFCDlg(CWnd* pParent)
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

BOOL CCalcStopwatchMFCDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    SetIcon(m_hIcon, TRUE);
    SetIcon(m_hIcon, FALSE);

    m_stwEverStarted = false;
    m_stwResetPending = false;

    SetDlgItemText(IDC_CAL_RESULT, m_calc.GetDisplay());
    SetDlgItemText(IDC_CAL_RS_HS, m_calc.GetRSHS());

    SetDlgItemText(IDC_TIME, m_stw.GetNowText());
    SetDlgItemText(IDC_STWC_RESULT, m_stw.GetElapsedText());

    if (GetDlgItem(IDC_STWC_RC))    GetDlgItem(IDC_STWC_RC)->EnableWindow(FALSE);
    if (GetDlgItem(IDC_STWC_CLEAR)) GetDlgItem(IDC_STWC_CLEAR)->EnableWindow(FALSE);

    m_laps.clear();

    m_lapList.ModifyStyle(0, LVS_REPORT);
    m_lapList.DeleteAllItems();
    while (m_lapList.DeleteColumn(0)) {}

    m_lapList.InsertColumn(0, _T("랩"), LVCFMT_LEFT, 140);
    m_lapList.InsertColumn(1, _T("시간"), LVCFMT_LEFT, 140);
    m_lapList.InsertColumn(2, _T("합계"), LVCFMT_LEFT, 140);

    DWORD ex = m_lapList.GetExtendedStyle();
    m_lapList.SetExtendedStyle(ex | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    m_calcExit = false;
    m_stwExit = false;

    m_calcThread = AfxBeginThread(&CCalcStopwatchMFCDlg::CalcThreadProc, this);
    if (m_calcThread) m_calcThread->m_bAutoDelete = FALSE;

    m_stwThread = AfxBeginThread(&CCalcStopwatchMFCDlg::StwThreadProc, this);
    if (m_stwThread) m_stwThread->m_bAutoDelete = FALSE;

    return TRUE;
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

        if (dlg->m_calcExit)
            break;

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

            if (!hasCmd)
                break;

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
    CString disp, hist;
    {
        CSingleLock lock(&m_calcStateCs, TRUE);
        disp = m_calc.GetDisplay();
        hist = m_calc.GetRSHS();
    }

    SetDlgItemText(IDC_CAL_RESULT, disp);
    SetDlgItemText(IDC_CAL_RS_HS, hist);
    return 0;
}

void CCalcStopwatchMFCDlg::OnCalcButtonRange(UINT nID)
{
    int idx = (int)nID - (int)IDC_CAL_BTN1;

    Calculator::Command cmd{};
    bool ok = true;

    switch (idx)
    {
    case 8:  cmd.kind = Calculator::CmdKind::Digit; cmd.digit = 7; break;
    case 9:  cmd.kind = Calculator::CmdKind::Digit; cmd.digit = 8; break;
    case 10: cmd.kind = Calculator::CmdKind::Digit; cmd.digit = 9; break;
    case 12: cmd.kind = Calculator::CmdKind::Digit; cmd.digit = 4; break;
    case 13: cmd.kind = Calculator::CmdKind::Digit; cmd.digit = 5; break;
    case 14: cmd.kind = Calculator::CmdKind::Digit; cmd.digit = 6; break;
    case 16: cmd.kind = Calculator::CmdKind::Digit; cmd.digit = 1; break;
    case 17: cmd.kind = Calculator::CmdKind::Digit; cmd.digit = 2; break;
    case 18: cmd.kind = Calculator::CmdKind::Digit; cmd.digit = 3; break;
    case 21: cmd.kind = Calculator::CmdKind::Digit; cmd.digit = 0; break;

    case 22: cmd.kind = Calculator::CmdKind::Decimal; break;

    case 7:  cmd.kind = Calculator::CmdKind::Op; cmd.op = Calculator::Op::Div; break;
    case 11: cmd.kind = Calculator::CmdKind::Op; cmd.op = Calculator::Op::Mul; break;
    case 15: cmd.kind = Calculator::CmdKind::Op; cmd.op = Calculator::Op::Sub; break;
    case 19: cmd.kind = Calculator::CmdKind::Op; cmd.op = Calculator::Op::Add; break;

    case 23: cmd.kind = Calculator::CmdKind::Equal; break;

    case 2:  cmd.kind = Calculator::CmdKind::Clear; break;
    case 1:  cmd.kind = Calculator::CmdKind::ClearEntry; break;
    case 3:  cmd.kind = Calculator::CmdKind::Backspace; break;

    case 0:  cmd.kind = Calculator::CmdKind::Percent; break;
    case 20: cmd.kind = Calculator::CmdKind::ToggleSign; break;

    case 4:  cmd.kind = Calculator::CmdKind::Reciprocal; break;
    case 5:  cmd.kind = Calculator::CmdKind::Square; break;
    case 6:  cmd.kind = Calculator::CmdKind::SqrtX; break;

    default:
        ok = false;
        break;
    }

    if (ok)
        EnqueueCalc(cmd);
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

        if (dlg->m_stwExit)
            break;

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

                if (!hasCmd)
                    break;

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

        if (running)
        {
            dlg->PostMessage(WM_APP_STW_UPDATED, 0, 0);
            continue;
        }

        if (processedAny || postedLap)
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
    CString now = m_stw.GetNowText();
    CString elapsed;
    bool running = false;
    bool resetPending = false;

    {
        CSingleLock lock(&m_stwStateCs, TRUE);
        running = m_stw.IsRunning();
        resetPending = m_stwResetPending;
        elapsed = resetPending ? _T("00:00:00.00") : m_stw.GetElapsedText();
    }

    SetDlgItemText(IDC_TIME, now);
    SetDlgItemText(IDC_STWC_RESULT, elapsed);

    const bool dirty = (!m_laps.empty() || m_stwEverStarted);

    if (GetDlgItem(IDC_STWC_RC))
        GetDlgItem(IDC_STWC_RC)->EnableWindow(running ? TRUE : FALSE);

    if (GetDlgItem(IDC_STWC_CLEAR))
        GetDlgItem(IDC_STWC_CLEAR)->EnableWindow(dirty ? TRUE : FALSE);

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

    int minIdx = -1;
    int maxIdx = -1;

    if (n >= 2)
    {
        LONGLONG minV = LLONG_MAX;
        LONGLONG maxV = LLONG_MIN;

        for (int i = 0; i < n; i++)
        {
            LONGLONG v = m_laps[i].lapCounter;
            if (v < minV) { minV = v; minIdx = i; }
            if (v > maxV) { maxV = v; maxIdx = i; }
        }

        if (minV == maxV)
        {
            minIdx = -1;
            maxIdx = -1;
        }
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
    if (!payload)
        return 0;

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

    if (!running)
        m_stwEverStarted = true;

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

    if (!running)
        return;

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
