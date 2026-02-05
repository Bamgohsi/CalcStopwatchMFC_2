#pragma once
#include <afxstr.h>

// 계산기 핵심 로직 클래스
// UI 요소 없이 순수 데이터와 연산만 처리
class Calculator
{
public:
    // 연산자 종류 (없음, 더하기, 빼기, 곱하기, 나누기)
    enum class Op { None, Add, Sub, Mul, Div };

    // 사용자가 내릴 수 있는 명령의 종류
    enum class CmdKind
    {
        Digit,      // 숫자 (0~9)
        Decimal,    // 소수점 (.)
        Op,         // 사칙연산자 (+, -, *, /)
        Equal,      // 결과 확인 (=)
        Clear,      // 전체 초기화 (C)
        ClearEntry, // 현재 입력만 초기화 (CE)
        Backspace,  // 한 글자 지우기
        Percent,    // 퍼센트 (%)
        ToggleSign, // 부호 변경 (+/-)
        Reciprocal, // 역수 (1/x)
        Square,     // 제곱 (x²)
        SqrtX       // 제곱근 (√x)
    };

    // UI에서 계산기 엔진으로 보낼 명령 패키지 구조체
    struct Command
    {
        CmdKind kind = CmdKind::Clear; // 무슨 명령인지
        int digit = 0;                 // 숫자 버튼일 경우 그 값
        Op op = Op::None;              // 연산자 버튼일 경우 그 종류
    };

public:
    Calculator();

    // 화면(큰 글씨)에 보여줄 문자열 반환
    CString GetDisplay() const;
    // 히스토리(작은 글씨)에 보여줄 문자열 반환
    CString GetRSHS() const;

    // 외부에서 명령을 실행하는 유일한 통로
    void Execute(const Command& cmd);

private:
    // 내부 상태 초기화 함수
    void Clear();

    // 문자열 <-> 실수 변환 및 포맷팅 헬퍼 함수들
    double  GetValue() const;
    void    SetValue(double v);
    CString FormatNumber(double v) const;

    // 에러(0으로 나누기 등) 발생 시 처리
    void SetError(const CString& msg);

    // 각 기능별 내부 구현 함수들
    void AppendDigit(int d);
    void AppendDecimal();
    void Backspace();
    void ToggleSign();

    void SetOperator(Op op);
    void Equal();
    void Percent();
    void ClearEntry();

    // 단항 연산(제곱, 제곱근 등) 처리용 열거형 및 함수
    enum class UnaryKind { Reciprocal, Square, Sqrt };
    void ApplyUnary(UnaryKind kind);
    void GetUnaryOperand(double& v, CString& argStr) const;
    void SetUnaryHistory(const CString& expr);

    void Reciprocal();
    void Square();
    void SqrtX();

    // 연산자 Enum을 문자열(+, - 등)로 변환
    CString OpSymbol(Op op) const;
    // 실제 사칙연산 수행 함수
    double  ApplyBinary(Op op, double a, double b, bool& err) const;

    // 히스토리 문자열 처리 헬퍼들
    bool EndsWithOpToken(const CString& s) const;
    void ReplaceTrailingOpToken(const CString& opToken);
    void History_Reset();
    bool History_IsJustEvaluated() const;
    void History_ClearIfJustEvaluated();
    void History_SetPending(double lhs, Op op);
    void History_SetPendingText(const CString& lhsText, Op op);
    void History_SetBinary(double lhs, Op op, const CString& rhsText, bool withEqual);
    void History_SetBinaryText(const CString& lhsText, Op op, const CString& rhsText, bool withEqual);
    void History_AppendRhsText(const CString& rhsText, bool withEqual);

private:
    // 상태 변수들
    CString m_buf;   // 현재 화면에 표시되는 메인 숫자 값
    CString m_bufo;  // 상단 히스토리 문자열

    bool m_newEntry = true; // 숫자를 입력할 때 새로 쓸지(true), 뒤에 붙일지(false) 결정

    double m_acc = 0.0;        // 연산자 누르기 전의 값 저장 (Accumulator)
    Op m_pendingOp = Op::None; // 아직 계산 안 되고 대기 중인 연산자 (예: 1 + 에서 '+')

    Op m_lastOp = Op::None;   // '=' 연타 기능을 위한 마지막 연산자
    double m_lastRhs = 0.0;   // '=' 연타 기능을 위한 마지막 우변 값

    double m_percentBase = 0.0; // '=' 직후 퍼센트 반복 계산 기준값

    bool m_error = false;              // 에러 상태 플래그
    bool m_afterPercent = false;       // % 연산 직후인지 (동작 제어용)
    bool m_afterUnary = false;         // 루트/제곱 등 단항 연산 직후인지 (백스페이스 방지용)
    bool   m_percentBaseValid = false; // base가 유효한지 여부
};