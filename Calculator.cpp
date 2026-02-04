#include "pch.h"
#include "Calculator.h"
#include <cmath>

Calculator::Calculator()
{
    Clear();
}

CString Calculator::GetDisplay() const
{
    return m_buf;
}

CString Calculator::GetRSHS() const
{
    return m_bufo;
}

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
        if (m_pendingOp == Op::None)
            m_bufo = _T("");

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
        m_buf.Format(_T("%d"), d);
    else
    {
        CString t;
        t.Format(_T("%d"), d);
        m_buf += t;
    }
}

void Calculator::AppendDecimal()
{
    if (m_error) return;

    if (m_afterPercent || m_afterUnary)
    {
        if (m_pendingOp == Op::None)
            m_bufo = _T("");

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
    if (m_newEntry) return;

    int len = m_buf.GetLength();
    if (len <= 1)
    {
        m_buf = _T("0");
        m_newEntry = true;
        return;
    }

    m_buf.Delete(len - 1, 1);
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

void Calculator::SetOperator(Op op)
{
    if (m_error) return;

    if (!m_bufo.IsEmpty())
    {
        CString tt = m_bufo;
        tt.TrimRight();
        if (!tt.IsEmpty() && tt.Right(1) == _T("="))
        {
            m_bufo = _T("");
            m_afterUnary = false;
            m_afterPercent = false;
        }
    }

    double entry = GetValue();

    if (m_pendingOp == Op::None)
    {
        m_acc = entry;
        m_pendingOp = op;

        if (!m_bufo.IsEmpty() && m_afterUnary)
            m_bufo = m_bufo + _T(" ") + OpSymbol(op);
        else
            m_bufo = m_buf + _T(" ") + OpSymbol(op);

        m_newEntry = true;
        return;
    }

    if (m_newEntry)
    {
        m_pendingOp = op;

        if (!m_bufo.IsEmpty())
        {
            CString t = m_bufo;
            t.TrimRight();

            int pos = t.ReverseFind(_T(' '));
            if (pos >= 0)
                m_bufo = t.Left(pos + 1) + OpSymbol(op);
            else
                m_bufo = FormatNumber(m_acc) + _T(" ") + OpSymbol(op);
        }
        else
        {
            m_bufo = FormatNumber(m_acc) + _T(" ") + OpSymbol(op);
        }
        return;
    }

    bool err = false;
    double res = ApplyBinary(m_pendingOp, m_acc, entry, err);
    if (err)
    {
        m_error = true;
        m_buf = _T("Error");
        m_bufo = _T("");
        m_pendingOp = Op::None;
        m_newEntry = true;
        return;
    }

    m_acc = res;
    SetValue(res);

    m_pendingOp = op;
    m_bufo = FormatNumber(m_acc) + _T(" ") + OpSymbol(op);
    m_newEntry = true;
}

void Calculator::Equal()
{
    if (m_error) return;

    if (m_pendingOp != Op::None)
    {
        double left = m_acc;

        double rhs = m_newEntry ? left : GetValue();
        CString rhsStr = m_newEntry ? FormatNumber(rhs) : m_buf;

        bool err = false;
        double res = ApplyBinary(m_pendingOp, left, rhs, err);
        if (err)
        {
            m_error = true;
            m_buf = _T("0으로 나눌 수 없습니다");
            m_bufo = _T("");
            m_pendingOp = Op::None;
            m_lastOp = Op::None;
            m_newEntry = true;
            m_afterUnary = false;
            m_afterPercent = false;
            return;
        }

        if (!m_bufo.IsEmpty())
        {
            CString t = m_bufo;
            t.TrimRight();

            CString opAdd = OpSymbol(Op::Add);
            CString opSub = OpSymbol(Op::Sub);
            CString opMul = OpSymbol(Op::Mul);
            CString opDiv = OpSymbol(Op::Div);

            bool endsWithOp =
                (!t.IsEmpty() && (
                    t.Right(opAdd.GetLength()) == opAdd ||
                    t.Right(opSub.GetLength()) == opSub ||
                    t.Right(opMul.GetLength()) == opMul ||
                    t.Right(opDiv.GetLength()) == opDiv
                    ));

            if (endsWithOp)
                m_bufo = t + _T(" ") + rhsStr;
            else
                m_bufo = t;
        }
        else
        {
            m_bufo = FormatNumber(left) + _T(" ") + OpSymbol(m_pendingOp) + _T(" ") + rhsStr;
        }

        m_bufo += _T(" =");

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
        double a = GetValue();
        bool err = false;
        double res = ApplyBinary(m_lastOp, a, m_lastRhs, err);
        if (err)
        {
            m_error = true;
            m_buf = _T("0으로 나눌 수 없습니다");
            m_bufo = _T("");
            m_lastOp = Op::None;
            m_newEntry = true;
            m_afterUnary = false;
            m_afterPercent = false;
            return;
        }

        m_bufo = FormatNumber(a) + _T(" ") + OpSymbol(m_lastOp) + _T(" ") + FormatNumber(m_lastRhs) + _T(" =");
        SetValue(res);

        m_acc = res;
        m_newEntry = true;
        m_afterUnary = false;
        m_afterPercent = false;
        return;
    }

    if (!m_bufo.IsEmpty())
    {
        CString t = m_bufo;
        t.TrimRight();
        if (!t.IsEmpty() && t.Right(1) == _T("="))
            return;
    }

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
    if (m_pendingOp == Op::Add || m_pendingOp == Op::Sub)
        rhs = m_acc * entry / 100.0;
    else
        rhs = entry / 100.0;

    m_bufo = FormatNumber(m_acc) + _T(" ") + OpSymbol(m_pendingOp) + _T(" ") + FormatNumber(rhs);
    SetValue(rhs);

    m_newEntry = false;
    m_afterPercent = true;
    m_afterUnary = false;
}


void Calculator::ClearEntry()
{
    if (m_error)
    {
        Clear();
        return;
    }

    if (m_pendingOp == Op::None)
    {
        m_buf = _T("0");
        m_bufo = _T("");
        m_newEntry = true;
        m_afterPercent = false;
        return;
    }

    m_buf = _T("0");
    m_newEntry = false;
    m_afterPercent = false;
}

void Calculator::Reciprocal()
{
    if (m_error) return;

    double v = 0.0;
    CString argStr;

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

    if (v == 0.0)
    {
        m_error = true;
        m_buf = _T("0으로 나눌 수 없습니다");
        m_bufo = _T("");
        m_pendingOp = Op::None;
        m_lastOp = Op::None;
        m_newEntry = true;
        m_afterPercent = false;
        m_afterUnary = false;
        return;
    }

    double res = 1.0 / v;
    SetValue(res);

    if (m_pendingOp != Op::None)
    {
        if (!m_bufo.IsEmpty())
            m_bufo = m_bufo + _T(" 1/(") + argStr + _T(")");
        else
            m_bufo = FormatNumber(m_acc) + _T(" ") + OpSymbol(m_pendingOp) + _T(" 1/(") + argStr + _T(")");
    }
    else
    {
        m_bufo = _T("1/(") + argStr + _T(")");
        m_lastOp = Op::None;
    }

    m_newEntry = false;
    m_afterPercent = false;
    m_afterUnary = true;
}

void Calculator::Square()
{
    if (m_error) return;

    double v = 0.0;
    CString argStr;

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

    double res = v * v;
    SetValue(res);

    if (m_pendingOp != Op::None)
    {
        if (!m_bufo.IsEmpty())
            m_bufo = m_bufo + _T(" sqr(") + argStr + _T(")");
        else
            m_bufo = FormatNumber(m_acc) + _T(" ") + OpSymbol(m_pendingOp) + _T(" sqr(") + argStr + _T(")");
    }
    else
    {
        m_bufo = _T("sqr(") + argStr + _T(")");
        m_lastOp = Op::None;
    }

    m_newEntry = false;
    m_afterPercent = false;
    m_afterUnary = true;
}

void Calculator::SqrtX()
{
    if (m_error) return;

    double v = 0.0;
    CString argStr;

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

    if (v < 0.0)
    {
        m_error = true;
        m_buf = _T("잘못된 입력입니다");
        m_bufo = _T("");
        m_pendingOp = Op::None;
        m_lastOp = Op::None;
        m_newEntry = true;
        m_afterPercent = false;
        m_afterUnary = false;
        return;
    }

    double res = std::sqrt(v);
    SetValue(res);

    CString root = _T("\u221A");

    if (m_pendingOp != Op::None)
    {
        if (!m_bufo.IsEmpty())
            m_bufo = m_bufo + _T(" ") + root + _T("(") + argStr + _T(")");
        else
            m_bufo = FormatNumber(m_acc) + _T(" ") + OpSymbol(m_pendingOp) + _T(" ") + root + _T("(") + argStr + _T(")");
    }
    else
    {
        m_bufo = root + _T("(") + argStr + _T(")");
        m_lastOp = Op::None;
    }

    m_newEntry = false;
    m_afterPercent = false;
    m_afterUnary = true;
}
