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

    double  GetValue() const;
    void    SetValue(double v);
    CString FormatNumber(double v) const;

    void SetError(const CString& msg);

    void AppendDigit(int d);
    void AppendDecimal();
    void Backspace();
    void ToggleSign();

    void SetOperator(Op op);
    void Equal();
    void Percent();
    void ClearEntry();

    enum class UnaryKind { Reciprocal, Square, Sqrt };
    void ApplyUnary(UnaryKind kind);
    void GetUnaryOperand(double& v, CString& argStr) const;
    void SetUnaryHistory(const CString& expr);

    void Reciprocal();
    void Square();
    void SqrtX();

    CString OpSymbol(Op op) const;
    double  ApplyBinary(Op op, double a, double b, bool& err) const;

    bool EndsWithOpToken(const CString& s) const;
    void ReplaceTrailingOpToken(const CString& opToken);

    // History (RSHS) helpers
    void History_Reset();
    bool History_IsJustEvaluated() const;
    void History_ClearIfJustEvaluated();
    void History_SetPending(double lhs, Op op);
    void History_SetPendingText(const CString& lhsText, Op op);
    void History_SetBinary(double lhs, Op op, const CString& rhsText, bool withEqual);
    void History_SetBinaryText(const CString& lhsText, Op op, const CString& rhsText, bool withEqual);
    void History_AppendRhsText(const CString& rhsText, bool withEqual);

private:
    CString m_buf;   // main display
    CString m_bufo;  // history (RSHS)

    bool m_newEntry = true;

    double m_acc = 0.0;
    Op m_pendingOp = Op::None;

    Op m_lastOp = Op::None;
    double m_lastRhs = 0.0;

    bool m_error = false;
    bool m_afterPercent = false;
    bool m_afterUnary = false;
};
