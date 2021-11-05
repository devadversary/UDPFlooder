#include "common.h"

/**
    @brief 차이 구하기
*/
unsigned long distance(unsigned long a, unsigned long b)
{
    if (a > b) return a - b;
    return b - a;
}

/**
    @brief 셔터 초기화
    @param pShutter 초기화할 셔터 포인터
    @param nDelay   인터벌 설정 (ms 단위)
    @param FirstPass 첫 체크는 무조건 걸리도록 설정
*/
void Shutter_Init(SHUTTER* pShutter, unsigned long nInterval, BOOL FirstPass)
{
    if (!pShutter) return;
    pShutter->nBaseTime = GetTickCount();
    if (FirstPass) pShutter->nBaseTime -= (nInterval+1); /*첫 체크시 무조건 걸리도록 기준시 조정*/
    pShutter->nInterval = nInterval;
}

/**
    @brief 타이밍이 되었는지 체크
    @return 기준 시간이 되었으면 TRUE
*/
BOOL Shutter_Check(SHUTTER* pShutter)
{
    unsigned long long nCur;

    nCur = GetTickCount();
    if (distance(nCur, pShutter->nBaseTime) < pShutter->nInterval) return FALSE;
    pShutter->nBaseTime = nCur;
    return TRUE;
}
