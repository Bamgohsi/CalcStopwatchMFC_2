#include "pch.h"
#include "Stopwatch.h"

// ge: 생성자 - 성능 주파수를 조회하고 초기화합니다.
Stopwatch::Stopwatch()
{
    LARGE_INTEGER f;
    ::QueryPerformanceFrequency(&f);
    m_freq = f.QuadPart;
    Reset();
}

// ge: 스탑워치 상태를 완전히 초기화합니다.
void Stopwatch::Reset()
{
    m_running = false;
    m_accum = 0;
    m_start = 0;
    m_lastLapTotal = 0;
    m_lapNo = 0;
}

// ge: 현재 스탑워치가 작동 중인지 반환합니다.
bool Stopwatch::IsRunning() const
{
    return m_running;
}

// ge: 현재 하드웨어 성능 카운터(Tick) 값을 반환합니다.
LONGLONG Stopwatch::NowCounter() const
{
    LARGE_INTEGER c;
    ::QueryPerformanceCounter(&c);
    return c.QuadPart;
}

// ge: 특정 시점(stamp)까지의 경과 틱(Tick)을 계산합니다.
LONGLONG Stopwatch::ElapsedCounterAt(LONGLONG stamp) const
{
    // 실행 중이면: 누적 시간 + (현재 시점 - 시작 시점)
    // 정지 상태면: 누적 시간만 반환
    return m_running ? (m_accum + (stamp - m_start)) : m_accum;
}

// ge: 시작/정지 토글 기능을 수행합니다. 정확도를 위해 타임스탬프를 인자로 받습니다.
void Stopwatch::ToggleStartStopAt(LONGLONG stamp)
{
    if (!m_running)
    {
        // 정지 -> 시작
        m_running = true;
        m_start = stamp;
        return;
    }

    // 시작 -> 정지: 흐른 시간을 누적 변수에 더함
    m_accum += (stamp - m_start);
    m_running = false;
}

// ge: 랩타임(구간 기록)을 측정하고 스냅샷을 반환합니다.
Stopwatch::LapSnapshot Stopwatch::LapAt(LONGLONG stamp)
{
    LapSnapshot snap{};

    const LONGLONG total = ElapsedCounterAt(stamp);
    const LONGLONG lap = total - m_lastLapTotal; // 전체 시간 - 이전 랩까지의 시간
    if (lap <= 0) return snap;

    m_lapNo++;
    m_lastLapTotal = total; // 기준점 갱신

    snap.lapNo = m_lapNo;
    snap.totalCounter = total;
    snap.lapCounter = lap;
    return snap;
}

// ge: 현재 시스템 시각(HH:MM:SS) 문자열을 반환합니다.
CString Stopwatch::GetNowText() const
{
    CTime t = CTime::GetCurrentTime();
    return t.Format(_T("%H:%M:%S"));
}

// ge: 1/100초 단위 정수를 받아 시:분:초.밀리초 형식으로 변환하는 내부 함수입니다.
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

// ge: 하드웨어 틱(Tick) 값을 시간 문자열로 변환합니다.
CString Stopwatch::FormatCounter(LONGLONG counter) const
{
    if (counter < 0) counter = 0;
    // 주파수를 이용해 틱을 1/100초 단위로 변환
    const LONGLONG hs = (counter * 100) / m_freq;
    return FormatHundredths(hs);
}

// ge: 현재 흐른 시간을 포맷팅된 문자열로 반환합니다.
CString Stopwatch::GetElapsedText() const
{
    const LONGLONG c = NowCounter();
    const LONGLONG total = ElapsedCounterAt(c);
    return FormatCounter(total);
}