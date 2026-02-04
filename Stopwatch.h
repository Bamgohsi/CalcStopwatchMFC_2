#pragma once
#include <afxstr.h>
#include <Windows.h>

class Stopwatch
{
public:
    struct LapSnapshot
    {
        int      lapNo = 0;
        LONGLONG totalCounter = 0;
        LONGLONG lapCounter = 0;
    };

public:
    Stopwatch();

    void Reset();

    void ToggleStartStopAt(LONGLONG stamp);
    LapSnapshot LapAt(LONGLONG stamp);

    bool IsRunning() const;

    CString GetNowText() const;
    CString GetElapsedText() const;

    CString FormatCounter(LONGLONG counter) const;

private:
    LONGLONG NowCounter() const;
    LONGLONG ElapsedCounterAt(LONGLONG stamp) const;
    CString  FormatHundredths(LONGLONG hundredths) const;

private:
    LONGLONG m_freq = 0;

    bool     m_running = false;
    LONGLONG m_accum = 0;
    LONGLONG m_start = 0;

    LONGLONG m_lastLapTotal = 0;
    int      m_lapNo = 0;
};
