#pragma once
#include <afxstr.h>
#include <Windows.h>

// QueryPerformanceCounter 기반 스톱워치
class Stopwatch
{
public:
    struct LapSnapshot
    {
        int      lapNo = 0;           // 랩 번호
        LONGLONG totalCounter = 0;    // 총 경과 틱
        LONGLONG lapCounter = 0;      // 이번 구간 틱
    };

public:
    Stopwatch();

    // 초기화
    void Reset();

    // 시작/멈춤 토글
    void ToggleStartStopAt(LONGLONG stamp);
    // 랩 기록
    LapSnapshot LapAt(LONGLONG stamp);

    // 실행 상태
    bool IsRunning() const;

    // 현재 시각/경과 시간
    CString GetNowText() const;
    CString GetElapsedText() const;

    // 내부 카운터 -> 시간 문자열
    CString FormatCounter(LONGLONG counter) const;

private:
    LONGLONG NowCounter() const;
    LONGLONG ElapsedCounterAt(LONGLONG stamp) const;
    CString  FormatHundredths(LONGLONG hundredths) const;

private:
    LONGLONG m_freq = 0; // 타이머 주파수

    bool     m_running = false;
    LONGLONG m_accum = 0;
    LONGLONG m_start = 0;

    LONGLONG m_lastLapTotal = 0;
    int      m_lapNo = 0;
};