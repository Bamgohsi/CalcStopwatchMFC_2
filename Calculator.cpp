#include "pch.h"
#include "Calculator.h"
#include <cmath>

// ge: 오류 표기용 문자열
static const CString kErrDivZero = _T("0으로 나눌 수 없습니다");
static const CString kErrInvalid = _T("잘못된 입력입니다");

// ge: 생성자 - 계산기 전체 초기화 수행
Calculator::Calculator()
{
    Clear(); // ge: 초기화
}

// ge: 현재 결과창(Display) 문자열 반환
CString Calculator::GetDisplay() const { return m_buf; }

// ge: 상단식/히스토리(RSHS) 문자열 반환
CString Calculator::GetRSHS() const { return m_bufo; }

// ge: 커맨드 패턴 - 명령 종류에 따라 적절한 내부 함수 호출
void Calculator::Execute(const Command& cmd)
{
    switch (cmd.kind)
    {
    case CmdKind::Digit:        AppendDigit(cmd.digit); break;   // ge: 숫자 입력
    case CmdKind::Decimal:      AppendDecimal(); break;          // ge: 소수점 입력
    case CmdKind::Op:           SetOperator(cmd.op); break;      // ge: 연산자 입력
    case CmdKind::Equal:        Equal(); break;                  // ge: 등호
    case CmdKind::Clear:        Clear(); break;                  // ge: 전체 초기화(C)
    case CmdKind::ClearEntry:   ClearEntry(); break;             // ge: 입력만 초기화(CE)
    case CmdKind::Backspace:    Backspace(); break;              // ge: 백스페이스
    case CmdKind::Percent:      Percent(); break;                // ge: 퍼센트
    case CmdKind::ToggleSign:   ToggleSign(); break;             // ge: 부호 전환(±)
    case CmdKind::Reciprocal:   Reciprocal(); break;             // ge: 1/x
    case CmdKind::Square:       Square(); break;                 // ge: x^2
    case CmdKind::SqrtX:        SqrtX(); break;                  // ge: √x
    default: break;
    }
}

// ge: 모든 상태를 초기화 (C 버튼)
void Calculator::Clear()
{
    m_buf = _T("0");
    m_bufo = _T("");
    m_newEntry = true;

    m_acc = 0.0;
    m_pendingOp = Op::None;
    m_lastOp = Op::None;
    m_lastRhs = 0.0;

    m_error = false;
    m_afterPercent = false;
    m_afterUnary = false;

    // ge: '=' 직후 퍼센트 반복 기준 초기화
    m_percentBase = 0.0;
    m_percentBaseValid = false;
}

// ge: 에러 메시지 설정 + 상태를 안전하게 정리
void Calculator::SetError(const CString& msg)
{
    m_error = true;
    m_buf = msg;
    m_bufo = _T("");
    m_pendingOp = Op::None;
    m_lastOp = Op::None;
    m_newEntry = true;
    m_afterPercent = false;
    m_afterUnary = false;

    // ge: 에러 발생 시 퍼센트 반복 기준도 무효화
    m_percentBase = 0.0;
    m_percentBaseValid = false;
}

// ge: 현재 m_buf 문자열을 실수(double)로 변환하여 반환
double Calculator::GetValue() const
{
    return _tstof(m_buf);
}

// ge: 실수를 깔끔한 문자열로 포맷팅 (소수점 정리 등)
CString Calculator::FormatNumber(double v) const
{
    // ge: 아주 작은 값은 0으로 스냅
    if (std::fabs(v) < 1e-15) v = 0.0;

    // ge: 정수에 매우 가까우면 정수로 스냅 (표시 깔끔하게)
    const double scale = (std::fabs(v) < 1.0) ? 1.0 : std::fabs(v);
    const double nearestInt = std::round(v);
    if (std::fabs(v - nearestInt) <= 1e-12 * scale)
        v = nearestInt;

    CString s;
    s.Format(_T("%.15g"), v);
    if (s == _T("-0")) s = _T("0");
    return s;
}

// ge: m_buf에 숫자 값을 표시 문자열로 세팅
void Calculator::SetValue(double v)
{
    m_buf = FormatNumber(v);
}

// ge: 숫자 입력 처리
void Calculator::AppendDigit(int d)
{
    if (d < 0 || d > 9) return;
    if (m_error) return;

    // ge: 특수 연산 직후에 숫자를 누르면 기존 식을 무시하고 새로 시작
    if (m_afterPercent || m_afterUnary)
    {
        if (m_pendingOp == Op::None) History_Reset();

        // ge: 새 입력 시작이므로 '='직후 퍼센트 반복 기준은 끊는다
        m_percentBaseValid = false;
        m_percentBase = 0.0;

        m_buf.Format(_T("%d"), d);
        m_newEntry = false;
        m_afterPercent = false;
        m_afterUnary = false;
        return;
    }

    // ge: 연산자 누른 직후 등, 새로 입력해야 하는 상태일 때
    if (m_newEntry)
    {
        m_buf.Format(_T("%d"), d);
        m_newEntry = false;
        return;
    }

    // ge: 0만 있으면 교체, 아니면 뒤에 붙임
    if (m_buf == _T("0"))
    {
        m_buf.Format(_T("%d"), d);
        return;
    }

    CString t;
    t.Format(_T("%d"), d);
    m_buf += t;
}

// ge: 소수점 입력 처리
void Calculator::AppendDecimal()
{
    if (m_error) return;

    // ge: 결과값 나온 직후 소수점 누르면 "0."으로 시작
    if (m_afterPercent || m_afterUnary)
    {
        if (m_pendingOp == Op::None) History_Reset();

        // ge: 새 입력 시작이므로 '='직후 퍼센트 반복 기준은 끊는다
        m_percentBaseValid = false;
        m_percentBase = 0.0;

        m_buf = _T("0.");
        m_newEntry = false;
        m_afterPercent = false;
        m_afterUnary = false;
        return;
    }

    // ge: 새 입력이면 "0."부터 시작
    if (m_newEntry)
    {
        m_buf = _T("0.");
        m_newEntry = false;
        return;
    }

    // ge: 이미 소수점이 있으면 무시
    if (m_buf.Find(_T('.')) >= 0) return;
    m_buf += _T(".");
}

// ge: 백스페이스 처리
void Calculator::Backspace()
{
    if (m_error) return;

    // ge: [중요] 계산 결과값(루트, % 등)이 떠있는 상태에선 백스페이스가 안 먹히도록 함 (표준 계산기 동작)
    if (m_afterUnary || m_afterPercent) return;

    // ge: 새 입력 상태면 아무 것도 지우지 않음
    if (m_newEntry) return;

    int len = m_buf.GetLength();
    if (len <= 1)
    {
        m_buf = _T("0");
        m_newEntry = true;
        return;
    }

    m_buf.Delete(len - 1, 1);
    if (m_buf == _T("-")) { m_buf = _T("0"); m_newEntry = true; }
}

// ge: +/- 부호 토글
void Calculator::ToggleSign()
{
    if (m_error) return;

    double v = GetValue();
    if (std::fabs(v) < 1e-15) return;

    if (m_buf.GetLength() > 0 && m_buf[0] == _T('-'))
        m_buf.Delete(0, 1);
    else
        m_buf = _T("-") + m_buf;
}

// ge: 연산자 기호 문자열(표준 계산기처럼 × ÷ 등) 반환
CString Calculator::OpSymbol(Op op) const
{
    switch (op)
    {
    case Op::Add: return _T("+");
    case Op::Sub: return _T("-");
    case Op::Mul: return _T("\u00D7"); // ge: 곱하기(×) 유니코드
    case Op::Div: return _T("\u00F7"); // ge: 나누기(÷) 유니코드
    default: return _T("");
    }
}

// ge: 실제 이항 연산 수행
double Calculator::ApplyBinary(Op op, double a, double b, bool& err) const
{
    err = false;
    switch (op)
    {
    case Op::Add: return a + b;
    case Op::Sub: return a - b;
    case Op::Mul: return a * b;
    case Op::Div:
        if (b == 0.0) { err = true; return 0.0; } // ge: 0으로 나누기 방지
        return a / b;
    default:
        return b;
    }
}

// ge: 히스토리 문자열이 연산자 토큰으로 끝나는지 검사 (연산자 교체용)
bool Calculator::EndsWithOpToken(const CString& s) const
{
    CString t = s;
    t.TrimRight();
    if (t.IsEmpty()) return false;

    const CString add = OpSymbol(Op::Add);
    const CString sub = OpSymbol(Op::Sub);
    const CString mul = OpSymbol(Op::Mul);
    const CString div = OpSymbol(Op::Div);

    return (t.Right(add.GetLength()) == add) ||
        (t.Right(sub.GetLength()) == sub) ||
        (t.Right(mul.GetLength()) == mul) ||
        (t.Right(div.GetLength()) == div);
}

// ge: 히스토리 끝의 연산자 토큰을 새 연산자 토큰으로 교체
void Calculator::ReplaceTrailingOpToken(const CString& opToken)
{
    CString t = m_bufo;
    t.TrimRight();

    int pos = t.ReverseFind(_T(' '));
    if (pos >= 0)
        m_bufo = t.Left(pos + 1) + opToken;
    else
        m_bufo = FormatNumber(m_acc) + _T(" ") + opToken;
}

// ge: 히스토리 초기화
void Calculator::History_Reset()
{
    m_bufo = _T("");
}

// ge: 히스토리가 방금 '='로 평가된 상태인지 검사
bool Calculator::History_IsJustEvaluated() const
{
    CString t = m_bufo;
    t.TrimRight();
    if (t.IsEmpty()) return false;
    return (t.Right(1) == _T("="));
}

// ge: '=' 직후라면 히스토리를 비움 (다음 새 계산 시작용)
void Calculator::History_ClearIfJustEvaluated()
{
    if (History_IsJustEvaluated())
        History_Reset();
}

// ge: "lhs op" 형태로 대기 상태 히스토리 생성
void Calculator::History_SetPending(double lhs, Op op)
{
    History_SetPendingText(FormatNumber(lhs), op);
}

// ge: "lhsText op" 형태로 대기 상태 히스토리 생성(텍스트 기반)
void Calculator::History_SetPendingText(const CString& lhsText, Op op)
{
    m_bufo = lhsText + _T(" ") + OpSymbol(op);
}

// ge: "lhs op rhs" 또는 "lhs op rhs =" 형태로 히스토리 생성(숫자 기반)
void Calculator::History_SetBinary(double lhs, Op op, const CString& rhsText, bool withEqual)
{
    History_SetBinaryText(FormatNumber(lhs), op, rhsText, withEqual);
}

// ge: "lhsText op rhsText" 또는 "lhsText op rhsText =" 형태로 히스토리 생성(텍스트 기반)
void Calculator::History_SetBinaryText(const CString& lhsText, Op op, const CString& rhsText, bool withEqual)
{
    m_bufo = lhsText + _T(" ") + OpSymbol(op) + _T(" ") + rhsText;
    if (withEqual) m_bufo += _T(" =");
}

// ge: 현재 히스토리 끝에 rhs 텍스트를 붙여 마무리(= 포함 여부 선택)
void Calculator::History_AppendRhsText(const CString& rhsText, bool withEqual)
{
    CString t = m_bufo;
    t.TrimRight();

    if (t.IsEmpty())
    {
        m_bufo = rhsText;
    }
    else if (EndsWithOpToken(t))
    {
        m_bufo = t + _T(" ") + rhsText;
    }
    else
    {
        m_bufo = t;
    }

    if (withEqual)
        m_bufo += _T(" =");
}

// ge: 사칙연산 버튼 눌렀을 때 처리
void Calculator::SetOperator(Op op)
{
    if (m_error) return;

    // ge: 연산자 입력은 새로운 식을 시작하는 흐름이므로 '% 반복 기준'은 끊는다
    m_percentBaseValid = false;
    m_percentBase = 0.0;

    History_ClearIfJustEvaluated();
    const CString opTok = OpSymbol(op);
    const double entry = GetValue();

    // ge: 식의 시작 (예: "3 +")
    if (m_pendingOp == Op::None)
    {
        m_acc = entry;
        m_pendingOp = op;

        if (!m_bufo.IsEmpty() && m_afterUnary)
            History_SetPendingText(m_bufo, op);
        else
            History_SetPendingText(m_buf, op);

        m_newEntry = true;
        return;
    }

    // ge: 연산자 교체 (예: + 눌렀다가 - 누름)
    if (m_newEntry)
    {
        m_pendingOp = op;
        if (m_bufo.IsEmpty()) History_SetPending(m_acc, op);
        else ReplaceTrailingOpToken(opTok);
        return;
    }

    // ge: 연쇄 계산 (예: "3 + 5" 상태에서 '*' 누름 -> 8 계산 후 * 대기)
    bool err = false;
    const double res = ApplyBinary(m_pendingOp, m_acc, entry, err);
    if (err) { SetError(kErrDivZero); return; }

    m_acc = res;
    SetValue(res);

    m_pendingOp = op;
    History_SetPending(m_acc, op);
    m_newEntry = true;
}

// ge: 등호(=) 처리
void Calculator::Equal()
{
    if (m_error) return;

    // ge: 일반적인 계산 (좌변 연산자 우변 = 결과)
    if (m_pendingOp != Op::None)
    {
        const double left = m_acc;
        const double rhs = m_newEntry ? left : GetValue();
        const CString rhsStr = m_newEntry ? FormatNumber(rhs) : m_buf;

        bool err = false;
        const double res = ApplyBinary(m_pendingOp, left, rhs, err);
        if (err) { SetError(kErrDivZero); return; }

        if (m_bufo.IsEmpty()) History_SetBinary(left, m_pendingOp, rhsStr, true);
        else History_AppendRhsText(rhsStr, true);

        SetValue(res);

        m_acc = res;
        m_lastOp = m_pendingOp;
        m_lastRhs = rhs;

        m_pendingOp = Op::None;
        m_newEntry = true;
        m_afterPercent = false;
        m_afterUnary = false;

        // ge: '=' 직후 퍼센트 반복은 "첫 %"에서 기준값을 잡아야 하므로 여기선 valid를 false로 둠
        m_percentBaseValid = false;
        m_percentBase = 0.0;

        return;
    }

    // ge: 엔터 연타 시 이전 연산 반복
    if (m_lastOp != Op::None)
    {
        const double a = GetValue();
        bool err = false;
        const double res = ApplyBinary(m_lastOp, a, m_lastRhs, err);
        if (err) { SetError(kErrDivZero); return; }

        History_SetBinary(a, m_lastOp, FormatNumber(m_lastRhs), true);
        SetValue(res);

        m_acc = res;
        m_newEntry = true;
        m_afterUnary = false;
        m_afterPercent = false;

        // ge: 반복 '=' 이후에도 % 기준은 새로 잡게(첫 %에서) 만들기 위해 무효화
        m_percentBaseValid = false;
        m_percentBase = 0.0;

        return;
    }

    // ge: 이미 '=' 상태면 아무 것도 하지 않음
    if (History_IsJustEvaluated()) return;

    // ge: 단항 연산 표현이 있으면 그 뒤에 '=' 표시
    if (!m_bufo.IsEmpty() && m_afterUnary) m_bufo += _T(" =");
    else m_bufo = m_buf + _T(" =");

    m_acc = GetValue();
    m_newEntry = true;
    m_afterPercent = false;
    m_afterUnary = false;

    // ge: '=' 직후 퍼센트 반복은 "첫 %"에서 기준값을 잡아야 하므로 valid는 false
    m_percentBaseValid = false;
    m_percentBase = 0.0;
}

// ge: 퍼센트(%) 처리
void Calculator::Percent()
{
    if (m_error) return;

    // ge: 연산자 없이 %만 누른 경우
    // ge: - 표준 계산기 동작(사용자 요구): '=' 직후에는 "기준값(base)의 %를 반복 적용"
    // ge:   예) 10 = %  -> 10의 10%  = 1
    // ge:       다시 % -> 1의 10%   = 0.1
    // ge:       9 = %   -> 9의 9%   = 0.81
    // ge:       다시 % -> 0.81의 9% = 0.0729
    // ge: - 그 외(그냥 10 % 같은 입력)는 0으로 초기화
    if (m_pendingOp == Op::None)
    {
        // ge: '=' 직후이거나, 이미 % 반복 상태면(base 유지) 반복 퍼센트 적용
        if (History_IsJustEvaluated() || (m_afterPercent && m_percentBaseValid))
        {
            // ge: 첫 %에서는 기준값(base)을 '=' 직후 값으로 저장
            if (!m_percentBaseValid)
            {
                m_percentBase = GetValue();
                m_percentBaseValid = true;
            }

            const double v = GetValue();
            const double out = v * (m_percentBase / 100.0);

            // ge: 히스토리 표기(간단): "base %" 형태
            m_bufo = FormatNumber(m_percentBase) + _T(" %");

            SetValue(out);

            m_newEntry = false;
            m_afterPercent = true;
            m_afterUnary = false;
            return;
        }

        // ge: 일반적인 "연산자 없이 %"는 0으로 초기화
        SetValue(0.0);
        m_bufo = _T("0");

        m_newEntry = true;
        m_afterPercent = false;
        m_afterUnary = false;

        // ge: 기준값도 무효화
        m_percentBaseValid = false;
        m_percentBase = 0.0;

        return;
    }

    // ge: 우항 입력 중 %를 누른 경우 (예: 10 + 5 %)
    // ge: 만약 숫자를 아직 입력하지 않았다면 좌항(m_acc)을 우항으로 간주 (예: 10 + %)
    const double entry = m_newEntry ? m_acc : GetValue();
    double rhs = 0.0;

    // ge: 더하기/빼기는 비율 계산(좌항 * 우항 / 100), 곱하기/나누기는 산술 계산(우항 / 100)
    if (m_pendingOp == Op::Add || m_pendingOp == Op::Sub)
    {
        rhs = m_acc * entry / 100.0;
    }
    else
    {
        rhs = entry / 100.0;
    }

    // ge: 계산된 우항(rhs)을 히스토리에 기록하고 화면에 표시
    History_SetBinary(m_acc, m_pendingOp, FormatNumber(rhs), false);
    SetValue(rhs);

    // ge: % 계산 후 상태 업데이트
    // ge: 결과값(rhs)을 유지한 채 다음 연산(+, - 등)이나 등호(=)를 기다림
    m_newEntry = false;
    m_afterPercent = true;
    m_afterUnary = false;

    // ge: 이항 퍼센트는 '='직후 반복 퍼센트 규칙과 섞이면 안 되므로 기준값 무효화
    m_percentBaseValid = false;
    m_percentBase = 0.0;
}

// ge: CE(입력 초기화) 처리
void Calculator::ClearEntry()
{
    if (m_error) { Clear(); return; }

    if (m_pendingOp == Op::None)
    {
        m_buf = _T("0");
        History_Reset();
    }
    else
    {
        m_buf = _T("0");
    }

    m_newEntry = true;
    m_afterPercent = false;
    m_afterUnary = false;

    // ge: 입력을 초기화하면 퍼센트 반복 기준도 끊는다
    m_percentBaseValid = false;
    m_percentBase = 0.0;
}

// ge: 단항 연산에서 사용할 피연산자(v)와 표기 문자열(argStr)을 준비
void Calculator::GetUnaryOperand(double& v, CString& argStr) const
{
    if (m_pendingOp != Op::None && m_newEntry)
    {
        v = m_acc;
        argStr = FormatNumber(m_acc);
    }
    else
    {
        v = GetValue();
        argStr = m_buf;
    }
}

// ge: 단항 연산 결과를 히스토리에 표기(현재 식/연산자 상태에 맞춰 결합)
void Calculator::SetUnaryHistory(const CString& expr)
{
    if (m_pendingOp != Op::None)
    {
        if (!m_bufo.IsEmpty())
            m_bufo = m_bufo + _T(" ") + expr;
        else
            m_bufo = FormatNumber(m_acc) + _T(" ") + OpSymbol(m_pendingOp) + _T(" ") + expr;
    }
    else
    {
        m_bufo = expr;
        m_lastOp = Op::None;
    }
}

// ge: 단항 연산(1/x, 제곱, 제곱근) 공통 처리
void Calculator::ApplyUnary(UnaryKind kind)
{
    if (m_error) return;

    double v = 0.0;
    CString argStr;
    GetUnaryOperand(v, argStr);

    switch (kind)
    {
    case UnaryKind::Reciprocal: // ge: 1/x
        if (v == 0.0) { SetError(kErrDivZero); return; }
        SetValue(1.0 / v);
        SetUnaryHistory(_T("1/(") + argStr + _T(")"));
        break;

    case UnaryKind::Square: // ge: 제곱
        SetValue(v * v);
        SetUnaryHistory(_T("sqr(") + argStr + _T(")"));
        break;

    case UnaryKind::Sqrt: // ge: 제곱근
        if (v < 0.0) { SetError(kErrInvalid); return; }
        SetValue(std::sqrt(v));
        {
            const CString root = _T("\u221A");
            SetUnaryHistory(root + _T("(") + argStr + _T(")"));
        }
        break;

    default:
        return;
    }

    // ge: 단항 연산 직후는 "결과 표시 중" 상태로 둠
    m_newEntry = false;
    m_afterPercent = false;
    m_afterUnary = true;

    // ge: 단항 결과가 나온 상태에서 '='직후 퍼센트 반복 기준은 의미가 불명확하므로 끊는다
    m_percentBaseValid = false;
    m_percentBase = 0.0;
}

// ge: 1/x 실행
void Calculator::Reciprocal() { ApplyUnary(UnaryKind::Reciprocal); }

// ge: 제곱 실행
void Calculator::Square() { ApplyUnary(UnaryKind::Square); }

// ge: 제곱근 실행
void Calculator::SqrtX() { ApplyUnary(UnaryKind::Sqrt); }