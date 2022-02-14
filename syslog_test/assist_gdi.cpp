#include "assist_gdi.h"

/**
	@brief 텍스트의 출력 영역크기 얻기
*/
void Draw_GetTextExtent(HDC hDC, char* Str, HFONT hFont, SIZE* pSize)
{
	HFONT hOldFont;

	hOldFont = (HFONT)SelectObject(hDC, hFont);
	GetTextExtentPoint32A(hDC, Str, (int)strlen(Str), pSize); /*텍스트의 폰트출력 크기 얻기*/
	SelectObject(hDC, hOldFont);
}

void Draw_Text(HDC hDC, char* Str, int x, int y, COLORREF TextColor, HFONT TextFont)
{
	HFONT hOldFont;
	COLORREF OldColor;

	if (!Str) return;
	hOldFont = (HFONT)SelectObject(hDC, TextFont);
	OldColor = SetTextColor(hDC, TextColor);

	TextOutA(hDC, x, y, Str, (int)strlen(Str));

	SetTextColor(hDC, OldColor);
	SelectObject(hDC, hOldFont);
}


void Draw_Rectangle(HDC hDC, int x, int y, int cx, int cy, COLORREF PenColor, COLORREF BrushColor, DWORD nullpen, DWORD nullbrush)
{
	HPEN hPen, hOP;
	HBRUSH hBr, hOB;

	if (nullpen) hPen = (HPEN)GetStockObject(NULL_PEN);
	else hPen = CreatePen(PS_SOLID, 1, PenColor);

	if (nullbrush) hBr = (HBRUSH)GetStockObject(NULL_BRUSH);
	else hBr = CreateSolidBrush(BrushColor);

	hOP = (HPEN)SelectObject(hDC, hPen);
	hOB = (HBRUSH)SelectObject(hDC, hBr);

	Rectangle(hDC, x, y, x + cx, y + cy);

	SelectObject(hDC, hOP);
	SelectObject(hDC, hOB);

	DeleteObject(hPen);
	DeleteObject(hBr);
	return;
}

void Draw_Circle(HDC hDC, int x, int y, int cx, int cy, COLORREF PenColor, COLORREF BrushColor, DWORD nullpen, DWORD nullbrush)
{
	HPEN hPen, hOP;
	HBRUSH hBr, hOB;

	if (nullpen) hPen = (HPEN)GetStockObject(NULL_PEN);
	else hPen = CreatePen(PS_SOLID, 1, PenColor);

	if (nullbrush) hBr = (HBRUSH)GetStockObject(NULL_BRUSH);
	else hBr = CreateSolidBrush(BrushColor);

	hOP = (HPEN)SelectObject(hDC, hPen);
	hOB = (HBRUSH)SelectObject(hDC, hBr);

	Ellipse(hDC, x, y, x + cx, y + cy);

	SelectObject(hDC, hOP);
	SelectObject(hDC, hOB);

	DeleteObject(hPen);
	DeleteObject(hBr);
	return;
}


void Draw_Arc(HDC hDC, int x, int y, int r, COLORREF PenColor, DWORD size, DWORD nullpen, float rad, float theta)
{
	HPEN hPen, hOP;

	if (nullpen) hPen = (HPEN)GetStockObject(NULL_PEN);
	else hPen = CreatePen(PS_SOLID, size, PenColor);

	hOP = (HPEN)SelectObject(hDC, hPen);

	Arc(hDC, x - r, y - r, x + r, y + r, (int)(x + r * cos(theta)), (int)(y - r * sin(theta)), (int)(x + r * cos(rad + theta)), (int)(y - r * sin(rad + theta)));

	SelectObject(hDC, hOP);

	DeleteObject(hPen);
	return;
}

void Draw_Bitmap(HDC hDC, HBITMAP hBit, int bit_size_x, int bit_size_y, int x, int y, int cx, int cy, int src_x, int src_y)
{
	HBITMAP hOldBit, hDB;
	HDC MemDC = CreateCompatibleDC(hDC);
	hDB = CreateCompatibleBitmap(hDC, bit_size_x, bit_size_y);

	hOldBit = (HBITMAP)SelectObject(MemDC, hBit);

	BitBlt(hDC, x, y, cx, cy, MemDC, src_x, src_y, SRCCOPY);

	SelectObject(MemDC, hOldBit);
	DeleteObject(hDB);
	DeleteDC(MemDC);
	return;
}