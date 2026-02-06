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
    // 공통 상태
    void Clear();
    void SetError(const CString& msg);

    // 표시/정규화
    double  GetValue() const;
    void    SetValue(double v);
    CString FormatNumber(double v) const;

    // "0.00000" 같은 입력을 히스토리/표시용으로 정규화(0으로 스냅)
    CString NormalizeNumericText(const CString& rawText, bool keepTrailingDot) const;

    // 사용자가 숫자를 입력해둔 상태에서 +/= 등을 누를 때 표시창도 정규화
    void NormalizeDisplayIfPossible();

    // '% 반복' 기준값 무효화
    void InvalidatePercentBase();

    // 특수연산 직후 입력 시작 공통 처리(중복 제거)
    bool BeginNewEntryFromSpecial(const CString& newText);

    // 입력 처리
    void AppendDigit(int d);
    void AppendDecimal();
    void Backspace();
    void ToggleSign();

    // 이항 처리
    void SetOperator(Op op);
    void Equal();
    void Percent();
    void ClearEntry();

    // 단항 처리
    enum class UnaryKind { Reciprocal, Square, Sqrt };
    void ApplyUnary(UnaryKind kind);
    void GetUnaryOperand(double& v, CString& argStr) const;
    void SetUnaryHistory(const CString& expr);

    void Reciprocal();
    void Square();
    void SqrtX();

    // 연산/히스토리
    CString OpSymbol(Op op) const;
    double  ApplyBinary(Op op, double a, double b, bool& err) const;

    bool EndsWithOpToken(const CString& s) const;
    void ReplaceTrailingOpToken(const CString& opToken);

    void History_Reset();
    bool History_IsJustEvaluated() const;
    void History_ClearIfJustEvaluated();

    // 숫자/텍스트 혼용으로 중복되던 함수들을 텍스트 기반으로만 통일
    void History_SetPendingText(const CString& lhsText, Op op);
    void History_SetBinaryText(const CString& lhsText, Op op, const CString& rhsText, bool withEqual);
    void History_AppendRhsText(const CString& rhsText, bool withEqual);

private:
    CString m_buf;   // 메인 표시
    CString m_bufo;  // 히스토리 표시

    bool m_newEntry = true;

    double m_acc = 0.0;
    Op m_pendingOp = Op::None;

    Op m_lastOp = Op::None;
    double m_lastRhs = 0.0;

    bool m_error = false;
    bool m_afterPercent = false;
    bool m_afterUnary = false;

    double m_percentBase = 0.0;
    bool m_percentBaseValid = false;
};
