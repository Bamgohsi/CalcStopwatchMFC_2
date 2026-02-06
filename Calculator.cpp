#include "pch.h"
#include "Calculator.h"
#include <cmath>
#include <tchar.h>

// 계산기 오류 메시지
static const CString kErrDivZero = _T("0으로 나눌 수 없습니다");
static const CString kErrInvalid = _T("입력이 잘못되었습니다.");

// 시작 시 전체 상태 초기화
Calculator::Calculator()
{
    Clear();
}

// 결과창 표시
CString Calculator::GetDisplay() const { return m_buf; }

// 상단식/히스토리 표시
CString Calculator::GetRSHS() const { return m_bufo; }

// 입력 명령 라우팅
void Calculator::Execute(const Command& cmd)
{
    switch (cmd.kind)
    {
    case CmdKind::Digit:        AppendDigit(cmd.digit); break;
    case CmdKind::Decimal:      AppendDecimal(); break;
    case CmdKind::Op:           SetOperator(cmd.op); break;
    case CmdKind::Equal:        Equal(); break;
    case CmdKind::Clear:        Clear(); break;
    case CmdKind::ClearEntry:   ClearEntry(); break;
    case CmdKind::Backspace:    Backspace(); break;
    case CmdKind::Percent:      Percent(); break;
    case CmdKind::ToggleSign:   ToggleSign(); break;
    case CmdKind::Reciprocal:   Reciprocal(); break;
    case CmdKind::Square:       Square(); break;
    case CmdKind::SqrtX:        SqrtX(); break;
    default: break;
    }
}

// '%' 반복 기준값 초기화
void Calculator::InvalidatePercentBase()
{
    m_percentBase = 0.0;
    m_percentBaseValid = false;
}

// 전체 초기화 (C)
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

    InvalidatePercentBase();
}

// 에러 상태로 전환
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

    InvalidatePercentBase();
}

// 표시 문자열 -> 값
double Calculator::GetValue() const
{
    return _tstof(m_buf);
}

// 값 -> 표시 문자열 (필요한 정규화 포함)
CString Calculator::FormatNumber(double v) const
{
    // 아주 작은 값은 0으로 정리
    if (std::fabs(v) < 1e-15) v = 0.0;

    // 정수에 매우 가까우면 정수로 정리
    const double scale = (std::fabs(v) < 1.0) ? 1.0 : std::fabs(v);
    const double nearestInt = std::round(v);
    if (std::fabs(v - nearestInt) <= 1e-12 * scale)
        v = nearestInt;

    CString s;
    s.Format(_T("%.15g"), v);
    if (s == _T("-0")) s = _T("0");
    return s;
}

// 결과창에 값 반영
void Calculator::SetValue(double v)
{
    m_buf = FormatNumber(v);
}

// 숫자 문자열 정규화 (0.00000 -> 0)
CString Calculator::NormalizeNumericText(const CString& rawText, bool keepTrailingDot) const
{
    CString t = rawText;
    t.Trim();

    if (t.IsEmpty()) return _T("0");

    // "2." 같은 입력은 필요하면 유지
    if (keepTrailingDot && t.Right(1) == _T(".")) return t;

    // 숫자 형태가 아니면 그대로 둠
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

// 연산 직전 표시 정리
void Calculator::NormalizeDisplayIfPossible()
{
    // 새 입력 시작 상태면 건드리지 않음
    if (m_newEntry) return;

    // 단항/퍼센트 직후는 표시 유지
    if (m_afterUnary || m_afterPercent) return;

    // 연산(=, 연산자 입력) 직전 trailing '.' 제거
    m_buf = NormalizeNumericText(m_buf, false);
}


// 특수 연산(%/단항) 직후 새 입력 시작 처리
bool Calculator::BeginNewEntryFromSpecial(const CString& newText)
{
    if (!(m_afterPercent || m_afterUnary)) return false;

    if (m_pendingOp == Op::None) History_Reset();

    InvalidatePercentBase();

    m_buf = newText;
    m_newEntry = false;
    m_afterPercent = false;
    m_afterUnary = false;
    return true;
}

// 숫자 입력
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

// 소수점 입력
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

// 백스페이스
void Calculator::Backspace()
{
    if (m_error) return;

    // 결과 표시 중(루트, %)에는 표준 계산기처럼 무시
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

// 부호 토글(±)
void Calculator::ToggleSign()
{
    if (m_error) return;

    // 이항 연산자 직후엔 RHS 입력 시작으로 처리
    if (m_pendingOp != Op::None && m_newEntry)
    {
        m_newEntry = false;
        m_afterPercent = false;
        m_afterUnary = false;
    }

    // '=' 직후라면 히스토리도 함께 갱신
    const bool evaluated = (m_pendingOp == Op::None) && History_IsJustEvaluated();

    double v = GetValue();
    if (std::fabs(v) < 1e-15)
    {
        // 0은 부호 변화 없음
        if (evaluated)
            m_bufo = NormalizeNumericText(m_buf, true) + _T(" =");
        return;
    }

    if (m_buf.GetLength() > 0 && m_buf[0] == _T('-'))
        m_buf.Delete(0, 1);
    else
        m_buf = _T("-") + m_buf;

    // '=' 직후 ± 토글 시 history 갱신
    if (evaluated)
        m_bufo = NormalizeNumericText(m_buf, true) + _T(" =");
}



// 연산자 기호 문자열
CString Calculator::OpSymbol(Op op) const
{
    switch (op)
    {
    case Op::Add: return _T("+");
    case Op::Sub: return _T("-");
    case Op::Mul: return _T("\u00D7");
    case Op::Div: return _T("\u00F7");
    default: return _T("");
    }
}

// 이항 연산 수행
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

// 히스토리 끝이 연산자 토큰인지 확인
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

// 히스토리 끝 연산자 교체
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

// 히스토리 초기화
void Calculator::History_Reset()
{
    m_bufo = _T("");
}

// 히스토리가 '='로 끝나는지 확인
bool Calculator::History_IsJustEvaluated() const
{
    CString t = m_bufo;
    t.TrimRight();
    if (t.IsEmpty()) return false;
    return (t.Right(1) == _T("="));
}

// '=' 직후면 히스토리 비움
void Calculator::History_ClearIfJustEvaluated()
{
    if (History_IsJustEvaluated())
        History_Reset();
}

// "lhs op" 형태로 대기 히스토리 생성
void Calculator::History_SetPendingText(const CString& lhsText, Op op)
{
    m_bufo = lhsText + _T(" ") + OpSymbol(op);
}

// "lhs op rhs" 또는 "lhs op rhs =" 형태로 히스토리 생성
void Calculator::History_SetBinaryText(const CString& lhsText, Op op, const CString& rhsText, bool withEqual)
{
    m_bufo = lhsText + _T(" ") + OpSymbol(op) + _T(" ") + rhsText;
    if (withEqual) m_bufo += _T(" =");
}

// 히스토리 끝에 rhs를 붙여 마무리
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

// 사칙연산 입력 처리
void Calculator::SetOperator(Op op)
{
    if (m_error) return;

    // 연산 직전 표시 정리
    NormalizeDisplayIfPossible();

    InvalidatePercentBase();
    History_ClearIfJustEvaluated();

    const CString opTok = OpSymbol(op);
    const double entry = GetValue();

    // 식 시작 (예: "3 +")
    if (m_pendingOp == Op::None)
    {
        m_acc = entry;
        m_pendingOp = op;

        // 히스토리는 정규화된 텍스트 사용
        if (!m_bufo.IsEmpty() && m_afterUnary)
            History_SetPendingText(m_bufo, op);
        else
            History_SetPendingText(NormalizeNumericText(m_buf, true), op);

        m_newEntry = true;
        return;
    }

    // 연산자 교체 (예: + -> -)
    if (m_newEntry)
    {
        m_pendingOp = op;
        if (m_bufo.IsEmpty()) History_SetPendingText(FormatNumber(m_acc), op);
        else ReplaceTrailingOpToken(opTok);
        return;
    }

    // 연쇄 계산 (예: "3 + 5" 상태에서 '*' 입력)
    bool err = false;
    const double res = ApplyBinary(m_pendingOp, m_acc, entry, err);
    if (err) { SetError(kErrDivZero); return; }

    m_acc = res;
    SetValue(res);

    m_pendingOp = op;
    History_SetPendingText(FormatNumber(m_acc), op);
    m_newEntry = true;
}

// '=' 처리
void Calculator::Equal()
{
    if (m_error) return;

    // 연산 직전 표시 정리
    NormalizeDisplayIfPossible();

    if (m_pendingOp != Op::None)
    {
        const double left = m_acc;
        const double rhs = m_newEntry ? left : GetValue();

        // 우항 표기도 정규화
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

    // 엔터 연타 시 이전 연산 반복
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

    // 단독 '='에서도 히스토리 정규화
    if (!m_bufo.IsEmpty() && m_afterUnary) m_bufo += _T(" =");
    else m_bufo = NormalizeNumericText(m_buf, true) + _T(" =");

    m_acc = GetValue();
    m_newEntry = true;
    m_afterPercent = false;
    m_afterUnary = false;

    InvalidatePercentBase();
}

// '%' 처리
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

            // base가 0이면 0 유지
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

// CE(입력 초기화)
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

// 단항 연산에 사용할 피연산자/표기 문자열 준비
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
        argStr = NormalizeNumericText(m_buf, true);
    }
}

// 단항 연산 결과를 히스토리에 결합
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

// 단항 연산 공통 처리
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

// 1/x
void Calculator::Reciprocal() { ApplyUnary(UnaryKind::Reciprocal); }
// 제곱
void Calculator::Square() { ApplyUnary(UnaryKind::Square); }
// 제곱근
void Calculator::SqrtX() { ApplyUnary(UnaryKind::Sqrt); }
