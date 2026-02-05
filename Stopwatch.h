#pragma once
#include <afxstr.h>
#include <Windows.h>

// QueryPerformanceCounter를 사용하는 정밀 스탑워치 클래스
class Stopwatch
{
public:
    // 랩타임 정보를 담는 구조체
    struct LapSnapshot
    {
        int      lapNo = 0;           // 랩 번호
        LONGLONG totalCounter = 0;    // 총 경과 틱(Tick)
        LONGLONG lapCounter = 0;      // 이번 구간 경과 틱
    };

public:
    Stopwatch();

    // 초기화
    void Reset();

    // 시작/멈춤 토글 (정밀도를 위해 클릭 시점의 Tick을 인자로 받음)
    void ToggleStartStopAt(LONGLONG stamp);
    // 랩타임 기록
    LapSnapshot LapAt(LONGLONG stamp);

    // 실행 중인지 확인
    bool IsRunning() const;

    // 현재 시각 문자열 (HH:MM:SS)
    CString GetNowText() const;
    // 경과 시간 문자열 (HH:MM:SS.ms)
    CString GetElapsedText() const;

    // 내부 틱 값을 시간 문자열로 변환
    CString FormatCounter(LONGLONG counter) const;

private:
    LONGLONG NowCounter() const;
    LONGLONG ElapsedCounterAt(LONGLONG stamp) const;
    CString  FormatHundredths(LONGLONG hundredths) const;

private:
    LONGLONG m_freq = 0; // CPU 타이머 주파수 (초당 틱 수)

    bool     m_running = false; // 실행 상태
    LONGLONG m_accum = 0;       // 누적된 시간 (정지했을 때 저장됨)
    LONGLONG m_start = 0;       // 현재 실행 구간의 시작 시간

    LONGLONG m_lastLapTotal = 0; // 직전 랩까지의 총 시간
    int      m_lapNo = 0;        // 랩 카운트
};