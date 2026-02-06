#include "pch.h"
#include "Stopwatch.h"

Stopwatch::Stopwatch()
{
    // ge: 시스템의 고해상도 타이머 주파수 획득
    LARGE_INTEGER f;
    ::QueryPerformanceFrequency(&f);
    m_freq = f.QuadPart;
    Reset();
}

void Stopwatch::Reset()
{
    m_running = false;
    m_accum = 0;
    m_start = 0;
    m_lastLapTotal = 0;
    m_lapNo = 0;
}

bool Stopwatch::IsRunning() const
{
    return m_running;
}

// ge: 현재 Tick 가져오기
LONGLONG Stopwatch::NowCounter() const
{
    LARGE_INTEGER c;
    ::QueryPerformanceCounter(&c);
    return c.QuadPart;
}

// ge: 특정 시점까지의 경과 시간 계산
LONGLONG Stopwatch::ElapsedCounterAt(LONGLONG stamp) const
{
    // 실행 중이면: 누적값 + (현재 - 시작)
    // 정지 중이면: 누적값만
    return m_running ? (m_accum + (stamp - m_start)) : m_accum;
}

// ge: 시작/정지 처리
void Stopwatch::ToggleStartStopAt(LONGLONG stamp)
{
    if (!m_running)
    {
        // 정지 -> 시작
        m_running = true;
        m_start = stamp;
        return;
    }

    // 시작 -> 정지 (흐른 시간을 누적함)
    m_accum += (stamp - m_start);
    m_running = false;
}

// ge: 랩타임 계산
Stopwatch::LapSnapshot Stopwatch::LapAt(LONGLONG stamp)
{
    LapSnapshot snap{};

    const LONGLONG total = ElapsedCounterAt(stamp);
    const LONGLONG lap = total - m_lastLapTotal; // 전체 - 이전랩
    if (lap <= 0) return snap;

    m_lapNo++;
    m_lastLapTotal = total;

    snap.lapNo = m_lapNo;
    snap.totalCounter = total;
    snap.lapCounter = lap;
    return snap;
}

// ge: 현재 시각 포맷팅
CString Stopwatch::GetNowText() const
{
    CTime t = CTime::GetCurrentTime();
    return t.Format(_T("%H:%M:%S"));
}

// ge: 1/100초 단위를 시:분:초.ms 로 변환
CString Stopwatch::FormatHundredths(LONGLONG hundredths) const
{
    if (hundredths < 0) hundredths = 0;

    const LONGLONG totalSec = hundredths / 100;
    const int hs = (int)(hundredths % 100);

    const int sec = (int)(totalSec % 60);
    const LONGLONG totalMin = totalSec / 60;

    const int min = (int)(totalMin % 60);
    const LONGLONG hour = totalMin / 60;

    CString s;
    s.Format(_T("%02lld:%02d:%02d.%02d"), hour, min, sec, hs);
    return s;
}

// ge: Tick -> 시간 문자열 변환
CString Stopwatch::FormatCounter(LONGLONG counter) const
{
    if (counter < 0) counter = 0;
    // Tick을 주파수로 나누어 1/100초 단위로 변환
    const LONGLONG hs = (counter * 100) / m_freq;
    return FormatHundredths(hs);
}

CString Stopwatch::GetElapsedText() const
{
    const LONGLONG c = NowCounter();
    const LONGLONG total = ElapsedCounterAt(c);
    return FormatCounter(total);
}