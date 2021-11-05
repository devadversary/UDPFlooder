#pragma once
#include <windows.h>

/**
    @brief 셔터 구조체 정의
    @remark 루프를 도는 중 특정 시간마다 실행 시켜야 할때 사용
*/
typedef struct _st_shutter
{
    unsigned long nInterval; /**< 밀리초 단위의 인터벌*/
    unsigned long nBaseTime; /**< 기준 시간*/
} SHUTTER;

/**
    @brief 차이 구하기
*/
unsigned long distance(unsigned long a, unsigned long b);

/**
    @brief 셔터 초기화
    @param pShutter 초기화할 셔터 포인터
    @param nDelay   인터벌 설정 (ms 단위)
    @param FirstPass 첫 체크는 무조건 걸리도록 설정
*/
void Shutter_Init(SHUTTER* pShutter, unsigned long nInterval, BOOL FirstPass);

/**
    @brief 타이밍이 되었는지 체크
    @return 기준 시간이 되었으면 TRUE
*/
BOOL Shutter_Check(SHUTTER* pShutter);