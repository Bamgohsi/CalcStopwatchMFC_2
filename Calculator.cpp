#include "pch.h"
#include "Calculator.h"
#include <cmath>

static const CString kErrDivZero = _T("0으로 나눌 수 없습니다");
static const CString kErrInvalid = _T("잘못된 입력입니다");

Calculator::Calculator()
{
    Clear();
}

CString Calculator::GetDisplay() const { return m_buf; }
CString Calculator::GetRSHS() const { return m_bufo; }

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
}

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
}

double Calculator::GetValue() const
{
    return _tstof(m_buf);
}

CString Calculator::FormatNumber(double v) const
{
    if (std::fabs(v) < 1e-15) v = 0.0;

    const double scale = (std::fabs(v) < 1.0) ? 1.0 : std::fabs(v);
    const double nearestInt = std::round(v);
    if (std::fabs(v - nearestInt) <= 1e-12 * scale)
        v = nearestInt;

    CString s;
    s.Format(_T("%.15g"), v);
    if (s == _T("-0")) s = _T("0");
    return s;
}

void Calculator::SetValue(double v)
{
    m_buf = FormatNumber(v);
}

void Calculator::AppendDigit(int d)
{
    if (d < 0 || d > 9) return;
    if (m_error) return;

    if (m_afterPercent || m_afterUnary)
    {
        if (m_pendingOp == Op::None) History_Reset();
        m_buf.Format(_T("%d"), d);
        m_newEntry = false;
        m_afterPercent = false;
        m_afterUnary = false;
        return;
    }

    if (m_newEntry)
    {
        m_buf.Format(_T("%d"), d);
        m_newEntry = false;
        return;
    }

    if (m_buf == _T("0"))
    {
        m_buf.Format(_T("%d"), d);
        return;
    }

    CString t;
    t.Format(_T("%d"), d);
    m_buf += t;
}

void Calculator::AppendDecimal()
{
    if (m_error) return;

    if (m_afterPercent || m_afterUnary)
    {
        if (m_pendingOp == Op::None) History_Reset();
        m_buf = _T("0.");
        m_newEntry = false;
        m_afterPercent = false;
        m_afterUnary = false;
        return;
    }

    if (m_newEntry)
    {
        m_buf = _T("0.");
        m_newEntry = false;
        return;
    }

    if (m_buf.Find(_T('.')) >= 0) return;
    m_buf += _T(".");
}

void Calculator::Backspace()
{
    if (m_error) return;

    // ge: 1/x, x², √x 같은 단항 연산이나 % 연산의 결과값이 화면에 있는 경우,
    // ge: 표준 계산기처럼 백스페이스로 숫자를 지울 수 없게 합니다.
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

void Calculator::History_Reset()
{
    m_bufo = _T("");
}

bool Calculator::History_IsJustEvaluated() const
{
    CString t = m_bufo;
    t.TrimRight();
    if (t.IsEmpty()) return false;
    return (t.Right(1) == _T("="));
}

void Calculator::History_ClearIfJustEvaluated()
{
    if (History_IsJustEvaluated())
        History_Reset();
}

void Calculator::History_SetPending(double lhs, Op op)
{
    History_SetPendingText(FormatNumber(lhs), op);
}

void Calculator::History_SetPendingText(const CString& lhsText, Op op)
{
    m_bufo = lhsText + _T(" ") + OpSymbol(op);
}

void Calculator::History_SetBinary(double lhs, Op op, const CString& rhsText, bool withEqual)
{
    History_SetBinaryText(FormatNumber(lhs), op, rhsText, withEqual);
}

void Calculator::History_SetBinaryText(const CString& lhsText, Op op, const CString& rhsText, bool withEqual)
{
    m_bufo = lhsText + _T(" ") + OpSymbol(op) + _T(" ") + rhsText;
    if (withEqual) m_bufo += _T(" =");
}

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

void Calculator::SetOperator(Op op)
{
    if (m_error) return;

    History_ClearIfJustEvaluated();
    const CString opTok = OpSymbol(op);
    const double entry = GetValue();

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

    if (m_newEntry)
    {
        m_pendingOp = op;
        if (m_bufo.IsEmpty()) History_SetPending(m_acc, op);
        else ReplaceTrailingOpToken(opTok);
        return;
    }

    bool err = false;
    const double res = ApplyBinary(m_pendingOp, m_acc, entry, err);
    if (err) { SetError(kErrDivZero); return; }

    m_acc = res;
    SetValue(res);

    m_pendingOp = op;
    History_SetPending(m_acc, op);
    m_newEntry = true;
}

void Calculator::Equal()
{
    if (m_error) return;

    if (m_pendingOp != Op::None)
    {
        const double left = m_acc;

        const double rhs = m_newEntry ? left : GetValue();
        const CString rhsStr = m_newEntry ? FormatNumber(rhs) : m_buf;

        bool err = false;
        const double res = ApplyBinary(m_pendingOp, left, rhs, err);
        if (err) { SetError(kErrDivZero); return; }

        if (m_bufo.IsEmpty())
        {
            History_SetBinary(left, m_pendingOp, rhsStr, true);
        }
        else
        {
            History_AppendRhsText(rhsStr, true);
        }

        SetValue(res);

        m_acc = res;
        m_lastOp = m_pendingOp;
        m_lastRhs = rhs;

        m_pendingOp = Op::None;
        m_newEntry = true;
        m_afterPercent = false;
        m_afterUnary = false;
        return;
    }

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
        return;
    }

    if (History_IsJustEvaluated())
        return;

    if (!m_bufo.IsEmpty() && m_afterUnary)
        m_bufo = m_bufo + _T(" =");
    else
        m_bufo = m_buf + _T(" =");

    m_acc = GetValue();
    m_newEntry = true;
    m_afterPercent = false;
    m_afterUnary = false;
}

void Calculator::Percent()
{
    if (m_error) return;

    if (m_pendingOp == Op::None)
    {
        const double v = GetValue();
        const double rhs = v / 100.0;

        m_bufo = FormatNumber(v) + _T(" %");
        SetValue(rhs);

        m_newEntry = false;
        m_afterPercent = true;
        m_afterUnary = false;
        return;
    }

    const double entry = m_newEntry ? 0.0 : GetValue();

    double rhs = 0.0;
    if (m_pendingOp == Op::Add || m_pendingOp == Op::Sub) rhs = m_acc * entry / 100.0;
    else rhs = entry / 100.0;

    History_SetBinary(m_acc, m_pendingOp, FormatNumber(rhs), false);
    SetValue(rhs);

    m_newEntry = false;
    m_afterPercent = true;
    m_afterUnary = false;
}

void Calculator::ClearEntry()
{
    if (m_error) { Clear(); return; }

    if (m_pendingOp == Op::None)
    {
        m_buf = _T("0");
        History_Reset();
        m_newEntry = true;
        m_afterPercent = false;
        m_afterUnary = false;
        return;
    }

    m_buf = _T("0");
    m_newEntry = false;
    m_afterPercent = false;
    m_afterUnary = false;
}

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
}

void Calculator::Reciprocal() { ApplyUnary(UnaryKind::Reciprocal); }
void Calculator::Square() { ApplyUnary(UnaryKind::Square); }
void Calculator::SqrtX() { ApplyUnary(UnaryKind::Sqrt); }