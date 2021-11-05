#pragma once
#include <windows.h>

/**
    @brief ���� ����ü ����
    @remark ������ ���� �� Ư�� �ð����� ���� ���Ѿ� �Ҷ� ���
*/
typedef struct _st_shutter
{
    unsigned long nInterval; /**< �и��� ������ ���͹�*/
    unsigned long nBaseTime; /**< ���� �ð�*/
} SHUTTER;

/**
    @brief ���� ���ϱ�
*/
unsigned long distance(unsigned long a, unsigned long b);

/**
    @brief ���� �ʱ�ȭ
    @param pShutter �ʱ�ȭ�� ���� ������
    @param nDelay   ���͹� ���� (ms ����)
    @param FirstPass ù üũ�� ������ �ɸ����� ����
*/
void Shutter_Init(SHUTTER* pShutter, unsigned long nInterval, BOOL FirstPass);

/**
    @brief Ÿ�̹��� �Ǿ����� üũ
    @return ���� �ð��� �Ǿ����� TRUE
*/
BOOL Shutter_Check(SHUTTER* pShutter);