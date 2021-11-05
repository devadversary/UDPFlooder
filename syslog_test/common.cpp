#include "common.h"

/**
    @brief ���� ���ϱ�
*/
unsigned long distance(unsigned long a, unsigned long b)
{
    if (a > b) return a - b;
    return b - a;
}

/**
    @brief ���� �ʱ�ȭ
    @param pShutter �ʱ�ȭ�� ���� ������
    @param nDelay   ���͹� ���� (ms ����)
    @param FirstPass ù üũ�� ������ �ɸ����� ����
*/
void Shutter_Init(SHUTTER* pShutter, unsigned long nInterval, BOOL FirstPass)
{
    if (!pShutter) return;
    pShutter->nBaseTime = GetTickCount();
    if (FirstPass) pShutter->nBaseTime -= (nInterval+1); /*ù üũ�� ������ �ɸ����� ���ؽ� ����*/
    pShutter->nInterval = nInterval;
}

/**
    @brief Ÿ�̹��� �Ǿ����� üũ
    @return ���� �ð��� �Ǿ����� TRUE
*/
BOOL Shutter_Check(SHUTTER* pShutter)
{
    unsigned long long nCur;

    nCur = GetTickCount();
    if (distance(nCur, pShutter->nBaseTime) < pShutter->nInterval) return FALSE;
    pShutter->nBaseTime = nCur;
    return TRUE;
}
