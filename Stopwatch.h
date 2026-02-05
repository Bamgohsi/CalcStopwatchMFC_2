#pragma once
#include <afxstr.h>
#include <Windows.h>

// ge: 고해상도 타이머를 사용하는 정밀 스탑워치 클래스입니다.
class Stopwatch
{
public:
    // ge: 특정 시점의 랩타임 정보를 저장하는 구조체입니다.
    struct LapSnapshot
    {
        int      lapNo = 0;           // ge: 랩 번호 (순서)
        LONGLONG totalCounter = 0;    // ge: 시작부터 현재까지의 누적 틱(Tick)
        LONGLONG lapCounter = 0;      // ge: 이전 랩부터 현재까지의 구간 틱(Tick)
    };

public:
    // ge: 생성자입니다. 타이머 주파수를 초기화합니다.
    Stopwatch();

    // ge: 스탑워치 상태를 초기화합니다.
    void Reset();

    // ge: 시작/정지를 토글합니다. 정확도를 위해 호출 시점의 틱(stamp)을 인자로 받습니다.
    void ToggleStartStopAt(LONGLONG stamp);
    // ge: 랩타임을 기록하고 해당 시점의 스냅샷을 반환합니다.
    LapSnapshot LapAt(LONGLONG stamp);

    // ge: 현재 스탑워치가 작동 중인지 여부를 반환합니다.
    bool IsRunning() const;

    // ge: 현재 시각 문자열(HH:MM:SS)을 반환합니다.
    CString GetNowText() const;
    // ge: 스탑워치의 경과 시간 문자열을 반환합니다.
    CString GetElapsedText() const;

    // ge: 틱(Tick) 카운트를 시간 포맷 문자열로 변환합니다.
    CString FormatCounter(LONGLONG counter) const;

private:
    // ge: 현재 시스템의 고해상도 카운터 값을 반환합니다.
    LONGLONG NowCounter() const;
    // ge: 특정 시점까지의 누적 경과 틱을 계산합니다.
    LONGLONG ElapsedCounterAt(LONGLONG stamp) const;
    // ge: 1/100초 단위 값을 시간 문자열로 포맷팅합니다.
    CString  FormatHundredths(LONGLONG hundredths) const;

private:
    LONGLONG m_freq = 0; // ge: 초당 틱(Tick) 수 (타이머 주파수)

    bool     m_running = false; // ge: 현재 실행 중인지 여부
    LONGLONG m_accum = 0;       // ge: 정지 상태일 때 저장된 누적 경과 시간(틱)
    LONGLONG m_start = 0;       // ge: 현재 실행 중인 구간의 시작 시점(틱)

    LONGLONG m_lastLapTotal = 0; // ge: 마지막으로 랩을 기록했을 때의 총 경과 시간
    int      m_lapNo = 0;        // ge: 현재 랩 번호
};