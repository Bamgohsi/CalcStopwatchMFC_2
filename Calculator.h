#pragma once
#include <afxstr.h>

class Calculator
{
public:
    enum class Op { None, Add, Sub, Mul, Div };

    enum class CmdKind
    {
        Digit,
        Decimal,
        Op,
        Equal,
        Clear,
        ClearEntry,
        Backspace,
        Percent,
        ToggleSign,
        Reciprocal,
        Square,
        SqrtX
    };

    struct Command
    {
        CmdKind kind = CmdKind::Clear;
        int digit = 0;
        Op op = Op::None;
    };

public:
    Calculator();

    CString GetDisplay() const;
    CString GetRSHS() const;

    void Execute(const Command& cmd);

private:
    void Clear();
    double GetValue() const;
    CString FormatNumber(double v) const;
    void SetValue(double v);

    void AppendDigit(int d);
    void AppendDecimal();
    void Backspace();
    void ToggleSign();

    void SetOperator(Op op);
    void Equal();
    void Percent();
    void ClearEntry();

    void Reciprocal();
    void Square();
    void SqrtX();

    CString OpSymbol(Op op) const;
    double ApplyBinary(Op op, double a, double b, bool& err) const;

private:
    CString m_buf;
    CString m_bufo;

    bool m_newEntry = true;

    double m_acc = 0.0;
    Op m_pendingOp = Op::None;

    Op m_lastOp = Op::None;
    double m_lastRhs = 0.0;

    bool m_error = false;
    bool m_afterPercent = false;
    bool m_afterUnary = false;
};
