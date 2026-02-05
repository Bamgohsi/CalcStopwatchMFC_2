#pragma once
#include <afxstr.h>

// ge: 계산기의 핵심 로직을 담당하는 클래스입니다. UI와 독립적으로 동작하며 상태를 관리합니다.
class Calculator
{
public:
    // ge: 사칙연산의 종류를 정의하는 열거형입니다.
    enum class Op { None, Add, Sub, Mul, Div };

    // ge: 계산기에 입력되는 명령(버튼)의 종류를 정의합니다.
    enum class CmdKind
    {
        Digit,      // ge: 숫자 (0~9)
        Decimal,    // ge: 소수점 (.)
        Op,         // ge: 연산자 (+, -, *, /)
        Equal,      // ge: 등호 (=)
        Clear,      // ge: 전체 초기화 (C)
        ClearEntry, // ge: 현재 입력 초기화 (CE)
        Backspace,  // ge: 한 글자 지우기 (<-)
        Percent,    // ge: 퍼센트 (%)
        ToggleSign, // ge: 부호 반전 (+/-)
        Reciprocal, // ge: 역수 (1/x)
        Square,     // ge: 제곱 (x²)
        SqrtX       // ge: 제곱근 (√x)
    };

    // ge: UI에서 계산기 로직으로 전달할 명령 정보를 담는 구조체입니다.
    struct Command
    {
        CmdKind kind = CmdKind::Clear; // ge: 명령의 종류
        int digit = 0;                 // ge: 숫자 버튼일 경우 해당 숫자 값
        Op op = Op::None;              // ge: 연산자 버튼일 경우 해당 연산자 종류
    };

public:
    // ge: 생성자입니다.
    Calculator();

    // ge: 현재 메인 디스플레이에 표시될 문자열을 반환합니다.
    CString GetDisplay() const;
    // ge: 상단에 표시될 계산 기록(History) 문자열을 반환합니다.
    CString GetRSHS() const;

    // ge: 외부(UI)로부터 명령을 받아 내부 상태를 갱신하는 메인 함수입니다.
    void Execute(const Command& cmd);

private:
    // ge: 계산기의 모든 상태를 초기화합니다.
    void Clear();

    // ge: 현재 버퍼의 문자열을 실수(double)로 변환하여 반환합니다.
    double  GetValue() const;
    // ge: 실수(double) 값을 문자열 포맷으로 변환하여 버퍼에 설정합니다.
    void    SetValue(double v);
    // ge: 실수를 깔끔한 문자열로 포맷팅하는 헬퍼 함수입니다.
    CString FormatNumber(double v) const;

    // ge: 에러 상태를 설정하고 메시지를 표시합니다.
    void SetError(const CString& msg);

    // ge: 각 버튼 기능별 내부 구현 함수들입니다.
    void AppendDigit(int d);    // ge: 숫자 추가
    void AppendDecimal();       // ge: 소수점 추가
    void Backspace();           // ge: 백스페이스 처리
    void ToggleSign();          // ge: 부호 변경

    void SetOperator(Op op);    // ge: 연산자 처리
    void Equal();               // ge: 등호 처리
    void Percent();             // ge: 퍼센트 처리
    void ClearEntry();          // ge: 입력 취소 처리

    // ge: 단항 연산(Unary)의 종류를 정의합니다.
    enum class UnaryKind { Reciprocal, Square, Sqrt };
    // ge: 단항 연산을 수행하는 공통 함수입니다.
    void ApplyUnary(UnaryKind kind);
    // ge: 단항 연산의 대상이 되는 값과 문자열을 가져옵니다.
    void GetUnaryOperand(double& v, CString& argStr) const;
    // ge: 단항 연산 수행 후 히스토리를 업데이트합니다.
    void SetUnaryHistory(const CString& expr);

    // ge: 구체적인 단항 연산 함수들입니다.
    void Reciprocal();
    void Square();
    void SqrtX();

    // ge: 연산자 타입을 문자열 기호로 변환합니다.
    CString OpSymbol(Op op) const;
    // ge: 두 수와 연산자를 받아 실제 계산을 수행합니다.
    double  ApplyBinary(Op op, double a, double b, bool& err) const;

    // ge: 문자열이 연산자로 끝나는지 확인합니다.
    bool EndsWithOpToken(const CString& s) const;
    // ge: 히스토리 끝의 연산자를 교체합니다.
    void ReplaceTrailingOpToken(const CString& opToken);

    // ge: 히스토리(RSHS) 관련 헬퍼 함수들입니다.
    void History_Reset();                                         // ge: 히스토리 초기화
    bool History_IsJustEvaluated() const;                         // ge: 방금 계산이 완료되었는지 확인
    void History_ClearIfJustEvaluated();                          // ge: 완료된 상태라면 히스토리 클리어
    void History_SetPending(double lhs, Op op);                   // ge: 대기 중인 연산 설정
    void History_SetPendingText(const CString& lhsText, Op op);   // ge: 텍스트로 대기 중인 연산 설정
    void History_SetBinary(double lhs, Op op, const CString& rhsText, bool withEqual); // ge: 이항 연산 히스토리 설정
    void History_SetBinaryText(const CString& lhsText, Op op, const CString& rhsText, bool withEqual); // ge: 텍스트로 이항 연산 설정
    void History_AppendRhsText(const CString& rhsText, bool withEqual); // ge: 우변 추가

private:
    CString m_buf;   // ge: 메인 디스플레이에 표시되는 숫자 문자열
    CString m_bufo;  // ge: 상단 히스토리 디스플레이에 표시되는 문자열

    bool m_newEntry = true; // ge: 다음 숫자 입력 시 새로운 수로 시작할지 여부

    double m_acc = 0.0;        // ge: 연산 대기 중인 왼쪽 피연산자 값 (Accumulator)
    Op m_pendingOp = Op::None; // ge: 대기 중인 연산자 (없으면 None)

    Op m_lastOp = Op::None;   // ge: 반복 계산(등호 연타)을 위한 마지막 연산자
    double m_lastRhs = 0.0;   // ge: 반복 계산을 위한 마지막 오른쪽 피연산자

    bool m_error = false;        // ge: 에러 상태 플래그 (0으로 나누기 등)
    bool m_afterPercent = false; // ge: 퍼센트 연산 직후인지 표시 (동작 제어용)
    bool m_afterUnary = false;   // ge: 단항 연산 직후인지 표시 (백스페이스 제어용)
};