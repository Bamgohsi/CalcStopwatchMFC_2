#include "pch.h"
#include "Stopwatch.h"

Stopwatch::Stopwatch()
{
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

LONGLONG Stopwatch::NowCounter() const
{
    LARGE_INTEGER c;
    ::QueryPerformanceCounter(&c);
    return c.QuadPart;
}

LONGLONG Stopwatch::ElapsedCounterAt(LONGLONG stamp) const
{
    return m_running ? (m_accum + (stamp - m_start)) : m_accum;
}

void Stopwatch::ToggleStartStopAt(LONGLONG stamp)
{
    if (!m_running)
    {
        m_running = true;
        m_start = stamp;
        return;
    }

    m_accum += (stamp - m_start);
    m_running = false;
}

Stopwatch::LapSnapshot Stopwatch::LapAt(LONGLONG stamp)
{
    LapSnapshot snap{};

    const LONGLONG total = ElapsedCounterAt(stamp);
    const LONGLONG lap = total - m_lastLapTotal;
    if (lap <= 0) return snap;

    m_lapNo++;
    m_lastLapTotal = total;

    snap.lapNo = m_lapNo;
    snap.totalCounter = total;
    snap.lapCounter = lap;
    return snap;
}

CString Stopwatch::GetNowText() const
{
    CTime t = CTime::GetCurrentTime();
    return t.Format(_T("%H:%M:%S"));
}

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

CString Stopwatch::FormatCounter(LONGLONG counter) const
{
    if (counter < 0) counter = 0;
    const LONGLONG hs = (counter * 100) / m_freq;
    return FormatHundredths(hs);
}

CString Stopwatch::GetElapsedText() const
{
    const LONGLONG c = NowCounter();
    const LONGLONG total = ElapsedCounterAt(c);
    return FormatCounter(total);
}
