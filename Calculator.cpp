#include "pch.h"
#include "Calculator.h"
#include <cmath>
#include <tchar.h>

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

// ge: '% 반복' 기준값(base)을 무효화
void Calculator::InvalidatePercentBase()
{
    m_percentBase = 0.0;
    m_percentBaseValid = false;
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

    InvalidatePercentBase(); // ge: '=' 직후 퍼센트 반복 기준 초기화
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

    InvalidatePercentBase(); // ge: 에러 발생 시 퍼센트 반복 기준도 무효화
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

// ge: 숫자 문자열을 히스토리/표시용으로 정규화(0.00000 -> 0)
CString Calculator::NormalizeNumericText(const CString& rawText, bool keepTrailingDot) const
{
    CString t = rawText;
    t.Trim();

    if (t.IsEmpty()) return _T("0");

    // ge: "2." 같은 입력은 표현 유지 옵션이 켜져 있으면 그대로 둠
    if (keepTrailingDot && t.Right(1) == _T(".")) return t;

    // ge: 숫자 형태가 아니면(에러 문자열 등) 그대로 사용
    bool hasDigit = false;
    for (int i = 0; i < t.GetLength(); ++i)
    {
        const TCHAR ch = t[i];
        if (_istdigit(ch)) { hasDigit = true; continue; }
        if (ch == _T('.') || ch == _T('-') || ch == _T('+') || ch == _T('e') || ch == _T('E')) continue;
        return t;
    }
    if (!hasDigit) return t;

    return FormatNumber(_tstof(t));
}

// ge: 사용자 입력이 숫자 형태면 표시창도 정규화(0.00000 -> 0)
void Calculator::NormalizeDisplayIfPossible()
{
    // ge: 새 입력 상태면 이미 0/정리된 상태이므로 건드리지 않음
    if (m_newEntry) return;

    // ge: 단항/% 결과 표시 중에는 표준 계산기처럼 표시 유지(원하면 여기서도 정규화 가능)
    if (m_afterUnary || m_afterPercent) return;

    m_buf = NormalizeNumericText(m_buf, true);
}

// ge: 특수 연산(%/단항) 직후 새 입력 시작 공통 처리
bool Calculator::BeginNewEntryFromSpecial(const CString& newText)
{
    if (!(m_afterPercent || m_afterUnary)) return false;

    if (m_pendingOp == Op::None) History_Reset();

    InvalidatePercentBase(); // ge: 새 입력 시작이므로 '% 반복 기준'은 끊는다

    m_buf = newText;
    m_newEntry = false;
    m_afterPercent = false;
    m_afterUnary = false;
    return true;
}

// ge: 숫자 입력 처리
void Calculator::AppendDigit(int d)
{
    if (d < 0 || d > 9) return;
    if (m_error) return;

    CString first;
    first.Format(_T("%d"), d);
    if (BeginNewEntryFromSpecial(first)) return;

    if (m_newEntry)
    {
        m_buf = first;
        m_newEntry = false;
        return;
    }

    if (m_buf == _T("0"))
    {
        m_buf = first;
        return;
    }

    m_buf += first;
}

// ge: 소수점 입력 처리
void Calculator::AppendDecimal()
{
    if (m_error) return;

    if (BeginNewEntryFromSpecial(_T("0."))) return;

    if (m_newEntry)
    {
        m_buf = _T("0.");
        m_newEntry = false;
        return;
    }

    if (m_buf.Find(_T('.')) >= 0) return;
    m_buf += _T(".");
}

// ge: 백스페이스 처리
void Calculator::Backspace()
{
    if (m_error) return;

    // ge: [중요] 계산 결과값(루트, % 등)이 떠있는 상태에선 백스페이스가 안 먹히도록 함 (표준 계산기 동작)
    if (m_afterUnary || m_afterPercent) return;

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
    case Op::Mul: return _T("\u00D7"); // ge: 곱하기(×)
    case Op::Div: return _T("\u00F7"); // ge: 나누기(÷)
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
        if (b == 0.0) { err = true; return 0.0; }
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

// ge: "lhsText op" 형태로 대기 상태 히스토리 생성(텍스트 기반)
void Calculator::History_SetPendingText(const CString& lhsText, Op op)
{
    m_bufo = lhsText + _T(" ") + OpSymbol(op);
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

    // ge: [수정] 0.00000 입력 후 + 눌렀을 때 표시창도 0으로 정규화
    NormalizeDisplayIfPossible();

    InvalidatePercentBase(); // ge: 연산자 입력은 새로운 식을 시작하는 흐름이므로 '% 반복 기준'은 끊는다
    History_ClearIfJustEvaluated();

    const CString opTok = OpSymbol(op);
    const double entry = GetValue();

    // ge: 식의 시작 (예: "3 +")
    if (m_pendingOp == Op::None)
    {
        m_acc = entry;
        m_pendingOp = op;

        // ge: [수정] 히스토리는 항상 정규화(0.00000 -> 0)
        if (!m_bufo.IsEmpty() && m_afterUnary)
            History_SetPendingText(m_bufo, op);
        else
            History_SetPendingText(NormalizeNumericText(m_buf, true), op);

        m_newEntry = true;
        return;
    }

    // ge: 연산자 교체 (예: + 눌렀다가 - 누름)
    if (m_newEntry)
    {
        m_pendingOp = op;
        if (m_bufo.IsEmpty()) History_SetPendingText(FormatNumber(m_acc), op);
        else ReplaceTrailingOpToken(opTok);
        return;
    }

    // ge: 연쇄 계산 (예: "3 + 5" 상태에서 '*' 누름)
    bool err = false;
    const double res = ApplyBinary(m_pendingOp, m_acc, entry, err);
    if (err) { SetError(kErrDivZero); return; }

    m_acc = res;
    SetValue(res);

    m_pendingOp = op;
    History_SetPendingText(FormatNumber(m_acc), op);
    m_newEntry = true;
}

// ge: 등호(=) 처리
void Calculator::Equal()
{
    if (m_error) return;

    // ge: [수정] 0.00000 입력 후 = 눌렀을 때 표시창도 0으로 정규화
    NormalizeDisplayIfPossible();

    if (m_pendingOp != Op::None)
    {
        const double left = m_acc;
        const double rhs = m_newEntry ? left : GetValue();

        // ge: [수정] 우항 표기도 정규화
        const CString rhsStr = m_newEntry ? FormatNumber(rhs) : NormalizeNumericText(m_buf, true);

        bool err = false;
        const double res = ApplyBinary(m_pendingOp, left, rhs, err);
        if (err) { SetError(kErrDivZero); return; }

        if (m_bufo.IsEmpty())
            History_SetBinaryText(FormatNumber(left), m_pendingOp, rhsStr, true);
        else
            History_AppendRhsText(rhsStr, true);

        SetValue(res);

        m_acc = res;
        m_lastOp = m_pendingOp;
        m_lastRhs = rhs;

        m_pendingOp = Op::None;
        m_newEntry = true;
        m_afterPercent = false;
        m_afterUnary = false;

        InvalidatePercentBase();
        return;
    }

    // ge: 엔터 연타 시 이전 연산 반복
    if (m_lastOp != Op::None)
    {
        const double a = GetValue();
        bool err = false;
        const double res = ApplyBinary(m_lastOp, a, m_lastRhs, err);
        if (err) { SetError(kErrDivZero); return; }

        History_SetBinaryText(FormatNumber(a), m_lastOp, FormatNumber(m_lastRhs), true);
        SetValue(res);

        m_acc = res;
        m_newEntry = true;
        m_afterUnary = false;
        m_afterPercent = false;

        InvalidatePercentBase();
        return;
    }

    if (History_IsJustEvaluated()) return;

    // ge: [수정] 단독 '='에서도 히스토리 숫자 정규화
    if (!m_bufo.IsEmpty() && m_afterUnary) m_bufo += _T(" =");
    else m_bufo = NormalizeNumericText(m_buf, true) + _T(" =");

    m_acc = GetValue();
    m_newEntry = true;
    m_afterPercent = false;
    m_afterUnary = false;

    InvalidatePercentBase();
}

// ge: 퍼센트(%) 처리
void Calculator::Percent()
{
    if (m_error) return;

    if (m_pendingOp == Op::None)
    {
        if (History_IsJustEvaluated() || (m_afterPercent && m_percentBaseValid))
        {
            if (!m_percentBaseValid)
            {
                m_percentBase = GetValue();
                m_percentBaseValid = true;
            }

            // ge: base가 0이면 0%는 의미 없으니 0 유지 + 히스토리 어지럽히지 않음
            if (std::fabs(m_percentBase) < 1e-15)
            {
                SetValue(0.0);
                if (!History_IsJustEvaluated()) m_bufo = _T("0");
                InvalidatePercentBase();

                m_newEntry = false;
                m_afterPercent = true;
                m_afterUnary = false;
                return;
            }

            const double v = GetValue();
            const double out = v * (m_percentBase / 100.0);

            m_bufo = FormatNumber(m_percentBase) + _T(" %");
            SetValue(out);

            m_newEntry = false;
            m_afterPercent = true;
            m_afterUnary = false;
            return;
        }

        SetValue(0.0);
        m_bufo = _T("0");
        m_newEntry = true;
        m_afterPercent = false;
        m_afterUnary = false;

        InvalidatePercentBase();
        return;
    }

    const double entry = m_newEntry ? m_acc : GetValue();
    double rhs = 0.0;

    if (m_pendingOp == Op::Add || m_pendingOp == Op::Sub)
        rhs = m_acc * entry / 100.0;
    else
        rhs = entry / 100.0;

    History_SetBinaryText(FormatNumber(m_acc), m_pendingOp, FormatNumber(rhs), false);
    SetValue(rhs);

    m_newEntry = false;
    m_afterPercent = true;
    m_afterUnary = false;

    InvalidatePercentBase();
}

// ge: CE(입력 초기화) 처리
void Calculator::ClearEntry()
{
    if (m_error) { Clear(); return; }

    m_buf = _T("0");
    if (m_pendingOp == Op::None) History_Reset();

    m_newEntry = true;
    m_afterPercent = false;
    m_afterUnary = false;

    InvalidatePercentBase();
}

// ge: 단항 연산에서 사용할 피연산자(v)와 표기 문자열(argStr)을 준비
void Calculator::GetUnaryOperand(double& v, CString& argStr) const
{
    if (m_pendingOp != Op::None && m_newEntry)
    {
        v = m_acc;
        argStr = FormatNumber(m_acc); // ge: 대기 상태(좌항 op)에서는 좌항을 대상으로
    }
    else
    {
        v = GetValue();
        argStr = NormalizeNumericText(m_buf, true); // ge: 히스토리 표기는 항상 정규화
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
    case UnaryKind::Reciprocal:
        if (v == 0.0) { SetError(kErrDivZero); return; }
        SetValue(1.0 / v);
        SetUnaryHistory(_T("1/(") + argStr + _T(")"));
        break;

    case UnaryKind::Square:
        SetValue(v * v);
        SetUnaryHistory(_T("sqr(") + argStr + _T(")"));
        break;

    case UnaryKind::Sqrt:
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

    m_newEntry = false;
    m_afterPercent = false;
    m_afterUnary = true;

    InvalidatePercentBase();
}

// ge: 1/x 실행
void Calculator::Reciprocal() { ApplyUnary(UnaryKind::Reciprocal); }
// ge: 제곱 실행
void Calculator::Square() { ApplyUnary(UnaryKind::Square); }
// ge: 제곱근 실행
void Calculator::SqrtX() { ApplyUnary(UnaryKind::Sqrt); }
