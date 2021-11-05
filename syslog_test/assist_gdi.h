#pragma once
#include <windows.h>
#include <math.h>

#define PI 3.141592

/**
	@brief 텍스트의 출력 영역크기 얻기
*/
void Draw_GetTextExtent(HDC hDC, char* Str, HFONT hFont, SIZE* pSize);

void Draw_Text(HDC hDC, char* Str, int x, int y, COLORREF TextColor, HFONT TextFont);

void Draw_Rectangle(HDC hDC, int x, int y, int cx, int cy, COLORREF PenColor, COLORREF BrushColor, DWORD nullpen, DWORD nullbrush);

void Draw_Circle(HDC hDC, int x, int y, int cx, int cy, COLORREF PenColor, COLORREF BrushColor, DWORD nullpen, DWORD nullbrush);

void Draw_Arc(HDC hDC, int x, int y, int r, COLORREF PenColor, DWORD size, DWORD nullpen, float rad, float theta);

void Draw_Bitmap(HDC hDC, HBITMAP hBit, int bit_size_x, int bit_size_y, int x, int y, int cx, int cy, int src_x, int src_y);