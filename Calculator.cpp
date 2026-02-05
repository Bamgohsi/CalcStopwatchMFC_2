#include "pch.h"
#include "Calculator.h"
#include <cmath>

static const CString kErrDivZero = _T("0으로 나눌 수 없습니다");
static const CString kErrInvalid = _T("잘못된 입력입니다");

// ge: 생성자 - 계산기를 초기 상태로 만듭니다.
Calculator::Calculator()
{
    Clear();
}

// ge: 화면에 표시할 메인 텍스트를 반환합니다.
CString Calculator::GetDisplay() const { return m_buf; }

// ge: 상단에 표시할 계산 기록(History) 텍스트를 반환합니다.
CString Calculator::GetRSHS() const { return m_bufo; }

// ge: 외부(UI)에서 전달된 명령을 수행하는 메인 함수입니다.
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

// ge: 계산기의 모든 데이터와 상태 플래그를 초기화합니다 (C 버튼).
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

// ge: 에러 발생 시 에러 메시지를 표시하고 계산을 중단시킵니다.
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

// ge: 현재 디스플레이 문자열을 double 실수형으로 변환합니다.
double Calculator::GetValue() const
{
    return _tstof(m_buf);
}

// ge: 실수를 문자열로 변환할 때 불필요한 소수점이나 미세한 오차를 정리합니다.
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

// ge: double 값을 포맷팅하여 디스플레이 버퍼에 저장합니다.
void Calculator::SetValue(double v)
{
    m_buf = FormatNumber(v);
}

// ge: 숫자 버튼(0~9)이 눌렸을 때의 처리를 담당합니다.
void Calculator::AppendDigit(int d)
{
    if (d < 0 || d > 9) return;
    if (m_error) return;

    // ge: 퍼센트나 단항 연산 결과가 나온 직후에 숫자를 누르면 새로운 입력으로 간주합니다.
    if (m_afterPercent || m_afterUnary)
    {
        if (m_pendingOp == Op::None) History_Reset();
        m_buf.Format(_T("%d"), d);
        m_newEntry = false;
        m_afterPercent = false;
        m_afterUnary = false;
        return;
    }

    // ge: 연산자 입력 후 첫 숫자라면 화면을 비우고 새로 씁니다.
    if (m_newEntry)
    {
        m_buf.Format(_T("%d"), d);
        m_newEntry = false;
        return;
    }

    // ge: 0만 있는 상태라면 숫자를 교체하고, 아니면 뒤에 이어 붙입니다.
    if (m_buf == _T("0"))
    {
        m_buf.Format(_T("%d"), d);
        return;
    }

    CString t;
    t.Format(_T("%d"), d);
    m_buf += t;
}

// ge: 소수점(.) 버튼이 눌렸을 때의 처리를 담당합니다.
void Calculator::AppendDecimal()
{
    if (m_error) return;

    // ge: 결과값 직후라면 "0."으로 새로 시작합니다.
    if (m_afterPercent || m_afterUnary)
    {
        if (m_pendingOp == Op::None) History_Reset();
        m_buf = _T("0.");
        m_newEntry = false;
        m_afterPercent = false;
        m_afterUnary = false;
        return;
    }

    // ge: 새로운 입력 시작이라면 "0."으로 시작합니다.
    if (m_newEntry)
    {
        m_buf = _T("0.");
        m_newEntry = false;
        return;
    }

    // ge: 이미 소수점이 있다면 무시합니다.
    if (m_buf.Find(_T('.')) >= 0) return;
    m_buf += _T(".");
}

// ge: 백스페이스(<-) 버튼 처리입니다.
void Calculator::Backspace()
{
    if (m_error) return;

    // ge: [수정됨] 단항 연산(1/x 등)이나 % 결과가 나온 상태에서는 지우기가 불가능하도록 막습니다. (표준 계산기 동작)
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
    // ge: 지웠는데 "-"만 남았다면 0으로 되돌립니다.
    if (m_buf == _T("-")) { m_buf = _T("0"); m_newEntry = true; }
}

// ge: 부호 반전(+/-) 버튼 처리입니다.
void Calculator::ToggleSign()
{
    if (m_error) return;

    double v = GetValue();
    if (std::fabs(v) < 1e-15) return; // 0은 부호를 바꾸지 않습니다.

    if (m_buf.GetLength() > 0 && m_buf[0] == _T('-'))
        m_buf.Delete(0, 1);
    else
        m_buf = _T("-") + m_buf;
}

// ge: 연산자 Enum을 문자열 기호(+, -, ×, ÷)로 변환합니다.
CString Calculator::OpSymbol(Op op) const
{
    switch (op)
    {
    case Op::Add: return _T("+");
    case Op::Sub: return _T("-");
    case Op::Mul: return _T("\u00D7"); // 곱하기 기호
    case Op::Div: return _T("\u00F7"); // 나누기 기호
    default: return _T("");
    }
}

// ge: 실제 사칙연산을 수행하는 내부 함수입니다. 0으로 나누기를 체크합니다.
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

// ge: 문자열이 연산자 기호로 끝나는지 확인하는 헬퍼 함수입니다.
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

// ge: 히스토리 문자열 끝의 연산자를 새로운 연산자로 교체합니다.
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

// ge: 히스토리를 초기화합니다.
void Calculator::History_Reset()
{
    m_bufo = _T("");
}

// ge: 히스토리가 계산 완료(=) 상태인지 확인합니다.
bool Calculator::History_IsJustEvaluated() const
{
    CString t = m_bufo;
    t.TrimRight();
    if (t.IsEmpty()) return false;
    return (t.Right(1) == _T("="));
}

// ge: 계산 완료 상태라면 히스토리를 지웁니다.
void Calculator::History_ClearIfJustEvaluated()
{
    if (History_IsJustEvaluated())
        History_Reset();
}

// ge: "숫자 + 연산자" 형태의 대기 중인 식을 히스토리에 설정합니다.
void Calculator::History_SetPending(double lhs, Op op)
{
    History_SetPendingText(FormatNumber(lhs), op);
}

// ge: 텍스트 기반으로 대기 중인 식을 설정합니다.
void Calculator::History_SetPendingText(const CString& lhsText, Op op)
{
    m_bufo = lhsText + _T(" ") + OpSymbol(op);
}

// ge: "좌변 연산자 우변 =" 형태의 완성된 식을 히스토리에 설정합니다.
void Calculator::History_SetBinary(double lhs, Op op, const CString& rhsText, bool withEqual)
{
    History_SetBinaryText(FormatNumber(lhs), op, rhsText, withEqual);
}

// ge: 텍스트 기반으로 완성된 식을 설정합니다.
void Calculator::History_SetBinaryText(const CString& lhsText, Op op, const CString& rhsText, bool withEqual)
{
    m_bufo = lhsText + _T(" ") + OpSymbol(op) + _T(" ") + rhsText;
    if (withEqual) m_bufo += _T(" =");
}

// ge: 히스토리에 우변과 등호를 추가합니다.
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

// ge: 사칙연산 버튼(+, -, *, /)을 처리합니다.
void Calculator::SetOperator(Op op)
{
    if (m_error) return;

    History_ClearIfJustEvaluated();
    const CString opTok = OpSymbol(op);
    const double entry = GetValue();

    // ge: 처음 연산자를 누른 경우 (예: 1 +)
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

    // ge: 연산자를 연속해서 누른 경우 (연산자 변경)
    if (m_newEntry)
    {
        m_pendingOp = op;
        if (m_bufo.IsEmpty()) History_SetPending(m_acc, op);
        else ReplaceTrailingOpToken(opTok);
        return;
    }

    // ge: 이미 대기 중인 연산이 있으면 계산을 수행하고 새 연산자를 설정합니다.
    bool err = false;
    const double res = ApplyBinary(m_pendingOp, m_acc, entry, err);
    if (err) { SetError(kErrDivZero); return; }

    m_acc = res;
    SetValue(res);

    m_pendingOp = op;
    History_SetPending(m_acc, op);
    m_newEntry = true;
}

// ge: 등호(=) 버튼을 처리합니다.
void Calculator::Equal()
{
    if (m_error) return;

    // ge: 대기 중인 연산이 있으면 계산합니다.
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
        return;
    }

    // ge: =을 반복해서 누를 때 이전 연산을 반복 수행합니다.
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

    // ge: 아무 연산 없이 =만 누른 경우
    if (History_IsJustEvaluated()) return;

    if (!m_bufo.IsEmpty() && m_afterUnary) m_bufo += _T(" =");
    else m_bufo = m_buf + _T(" =");

    m_acc = GetValue();
    m_newEntry = true;
    m_afterPercent = false;
    m_afterUnary = false;
}

// ge: 퍼센트(%) 버튼 처리입니다.
void Calculator::Percent()
{
    if (m_error) return;

    // ge: [수정됨] 연산자 없이 단독으로 사용되면 0으로 만듭니다 (표준 계산기 동작).
    if (m_pendingOp == Op::None)
    {
        SetValue(0.0);
        m_bufo = _T("0");

        m_newEntry = true;
        m_afterPercent = false; // 0이 되었으므로 일반 숫자로 취급
        m_afterUnary = false;
        return;
    }

    // ge: 연산 중간에 사용되면 앞 숫자에 대한 비율로 계산합니다.
    const double entry = m_newEntry ? 0.0 : GetValue();
    double rhs = 0.0;

    if (m_pendingOp == Op::Add || m_pendingOp == Op::Sub)
        rhs = m_acc * entry / 100.0;
    else
        rhs = entry / 100.0;

    History_SetBinary(m_acc, m_pendingOp, FormatNumber(rhs), false);
    SetValue(rhs);

    m_newEntry = false;
    m_afterPercent = true;
    m_afterUnary = false;
}

// ge: 현재 입력만 지우는 CE 버튼 처리입니다.
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

    m_newEntry = true; // CE 후에는 숫자를 새로 입력받도록 함
    m_afterPercent = false;
    m_afterUnary = false;
}

// ge: 단항 연산 대상 값을 가져옵니다.
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

// ge: 단항 연산 결과를 히스토리에 포맷팅하여 저장합니다.
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

// ge: 단항 연산(제곱, 제곱근, 역수)의 공통 로직을 수행합니다.
void Calculator::ApplyUnary(UnaryKind kind)
{
    if (m_error) return;

    double v = 0.0;
    CString argStr;
    GetUnaryOperand(v, argStr);

    switch (kind)
    {
    case UnaryKind::Reciprocal: // 1/x
        if (v == 0.0) { SetError(kErrDivZero); return; }
        SetValue(1.0 / v);
        SetUnaryHistory(_T("1/(") + argStr + _T(")"));
        break;

    case UnaryKind::Square: // x^2
        SetValue(v * v);
        SetUnaryHistory(_T("sqr(") + argStr + _T(")"));
        break;

    case UnaryKind::Sqrt: // sqrt(x)
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

// ge: 역수 버튼 핸들러
void Calculator::Reciprocal() { ApplyUnary(UnaryKind::Reciprocal); }
// ge: 제곱 버튼 핸들러
void Calculator::Square() { ApplyUnary(UnaryKind::Square); }
// ge: 제곱근 버튼 핸들러
void Calculator::SqrtX() { ApplyUnary(UnaryKind::Sqrt); }