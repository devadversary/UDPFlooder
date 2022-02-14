#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <windows.h>
#include <windowsx.h>
#include <process.h>
#include <stdio.h>

#include "common.h"
#include "assist_gdi.h"

#pragma comment (lib, "ws2_32.lib")

#define CLASSNAME TEXT("UDP_Flooder")
#define WIN_X (300)
#define WIN_Y (155)

#define ACTBUTTON_RECT {200,10,75,16}   /* ����/�����ư ��ǥ�� ũ�� */
#define THROTTLE_RECT  {10,35,265,30}   /* ����Ʋ �� ��ǥ�� ũ�� */
#define TIMER_RECT     {10,62,265,3}    /* Ÿ�̸� �� ��ǥ�� ũ�� */
#define MAX_TRAFFIC    (100)            /* Ʈ���� �Ѱ�ġ (���� Mbps) (12500000 ����Ʈ) */
#define MBPS_UNIT      (3)              /* Ʈ���� ���� ���� (3Mbps ������ ����)*/
#define MAX_THREAD     (4)              /* ������ ������ ���� */

typedef struct _st_APP_
{
    WSADATA          wsa;                /**< ���� ���̺귯�� �ڵ�*/
    HWND             hWndMain;           /**< ����������*/
    WNDPROC          fnMainProc;         /**< ���� ������ ���ν���*/
    HFONT            DefFont;            /**< ����� �⺻ ��Ʈ*/
    HFONT            SmallFont;          /**< ��¦ ���� ��Ʈ*/
    HWND             hEditIP;            /**< IP �Է� ����Ʈ �ڵ�*/
    HWND             hEditPort;          /**< Port �Է� ����Ʈ �ڵ�*/
    HWND             hEditTimer;         /**< Ÿ�̹� ����Ʈ �ڵ�*/

    BOOL             bFlagAvtivate;      /**< Ʈ���� ���� ��������. (TRUE�̸� ������)*/
    CRITICAL_SECTION cs;                 /**< ������ ����ȭ ��ü*/

    SOCKADDR_IN      TargetInfo;         /**< �׽�Ʈ ��� �ּ�����*/
    int              Mbps;               /**< ���� Ʈ���� ����Ʋ (0~300Mbps)*/
    int              AvgDatagramSize;    /**< ��� UDP �����ͱ׷� ũ��*/
    int              CurrentSendPackets; /**< ���� ��Ŷ��*/
    int              CurrentPPS;         /**< 1�ʴ� ��Ŷ��*/
    unsigned long    RunningTime;        /**< �׽�Ʈ �ð� (ms)*/
    unsigned long    BaseTick;           /**< �׽�Ʈ �ð��� ���ϱ����� ���� ƽ*/
    int              FloodTimer;         /**< �÷��� ���� �ð�*/

} APP;

/**
    @brief �÷��� ������ ���ڷ� �ѱ� ����ü
*/ 
typedef struct _st_threadarg_flood
{
    APP* pApp;  /* �� ���α׷��� ���� �ڵ� */
    int  Index; /* �� �������� ID ��. (� ���� ���ڿ��� �÷��� �� ������ �� ������ ����.)*/
} THREADARG_FLOOD;

/**
   @brief UDP�� ���� ���̵����� (������ Syslog �������̴�)
*/
char DummyLog[4][512] = {"Nov  4 10:35:05 deava666 whoopsie[1179]: [10:35:05] Not a paid data plan: /org/freedesktop/NetworkManager/ActiveConnection/1",
                         "Nov  4 11:00:24 Nova666 dbus-daemon[870]: [system] Activating via systemd: service name='org.freedesktop.nm_dispatcher' unit='dbus-org.freedesktop.nm-dispatcher.service' requested by ':1.12' (uid=0 pid=871 comm=\" / usr / sbin / NetworkManager --no - daemon \" label=\"unconfined\")",
                         "Nov  4 05:26:05 Lava666 NetworkManager[871]: <info>  [1635971165.1844] manager: NetworkManager state is now CONNECTED_GLOBAL",
                         "Nov  4 04:29:20 Java666 dbus-daemon[870]: [system] Activating via systemd: service name='org.freedesktop.fwupd' unit='fwupd.service' requested by ':1.96540' (uid=62803 pid=2796143 comm=\"/usr/bin/fwupdmgr refresh\"label=\"unconfined\")"};

/**
    @brief UDP ���Ͽ� ������
*/
unsigned int __stdcall Thread_Flood(void* temp)
{
    THREADARG_FLOOD* pArg = (THREADARG_FLOOD*)temp;
    SHUTTER Shutter;
    APP* pApp;
    int  Index;
    int  BytePerSec;
    SOCKET sock;
    int  nLenStr;
    char* pDummy;
    SOCKADDR targetadr;
    int TotalSend;
    int Tx;

    if (!pArg) return 0;
    pApp = pArg->pApp;
    Index = pArg->Index;

    Index %= MAX_THREAD; /*������ ������ �Ѿ�� �ε����� ������ �߶��ش�.*/
    sock = socket(AF_INET, SOCK_DGRAM, NULL);
    if (!sock) exit(-1);

    memcpy(&targetadr, &pApp->TargetInfo, sizeof(SOCKADDR));
    nLenStr = (int)strlen(DummyLog[Index]);
    pDummy = DummyLog[Index];
    TotalSend = 0;
    BytePerSec = 0;
    Shutter_Init(&Shutter, 1000, TRUE); /*1�� ���� üũ*/
    while(1){
        if (!pApp->bFlagAvtivate){ /*Ȱ��ȭ ���� ���� ���¶�� ��� ������ ����*/
            closesocket(sock);
            return 0;
        }

        if (Shutter_Check(&Shutter)){ /*���� üũ �ð� ���ƿ��� */
            BytePerSec = pApp->Mbps;  /* ���� �ð����� Mbps ���� */
            BytePerSec *= 1048576;    /* 1�ް� ���ϱ� */
            BytePerSec >>= 3;         /* ��Ʈ�� 8 ������ */
            BytePerSec /= MAX_THREAD; /* ������ �� ������ */
            TotalSend = 0;            /* ���� ����Ʈ �� �ٽ� �ʱ�ȭ */
        }

        if (TotalSend >= BytePerSec ) { 
            Sleep(1); /*��� ����*/
            continue; /*��Ŷ �۽� ����*/
        }
        
        Tx = sendto(sock, pDummy, nLenStr, 0, &targetadr, sizeof(SOCKADDR));
        /*�̴��� + IP + TCP/UDP ��� ���̱��� ���� ��ų������ ���� ����.*/
        Tx += (14 + 20 + 8); /*�̴���,IP,UDP ������̱��� �ջ�*/
        pApp->CurrentSendPackets ++;
        TotalSend += Tx;
    }
    return 0;
}

/**
    @brief �÷��� ��ư Ŭ���� ȣ��� �ڵ鷯
*/
static BOOL OnActivateButtonClick(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
    APP*            pApp;
    char            IPAddress[32];
    char            PortAddress[8];
    char            TimeStr[8];
    int             Port;
    RECT            TempRect;
    POINT           TempPt;
    int             i;

    static unsigned int    thID[MAX_THREAD];
    static THREADARG_FLOOD thArg[MAX_THREAD];

    pApp = (APP*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    memset(&pApp->TargetInfo, 0, sizeof(SOCKADDR));
    GetWindowTextA(pApp->hEditIP, IPAddress, sizeof(IPAddress));
    if ((pApp->TargetInfo.sin_addr.s_addr = inet_addr(IPAddress)) == INADDR_NONE) {
        MessageBoxA(hWnd, "��ȿ���� ���� �ּ� �Դϴ�.", "Error", MB_ICONWARNING);
        return FALSE;
    }
    GetWindowTextA(pApp->hEditPort, PortAddress, sizeof(PortAddress));
    Port = atoi(PortAddress);
    if (Port < 1 || Port > 65535) {
        MessageBoxA(hWnd, "��ȿ���� ���� ��Ʈ �Դϴ�.", "Error", MB_ICONWARNING);
        return FALSE;
    }
    pApp->TargetInfo.sin_port = htons(Port);
    pApp->TargetInfo.sin_family = AF_INET;
    TempRect = ACTBUTTON_RECT; TempRect.right += TempRect.left; TempRect.bottom += TempRect.top;
    TempPt.x = GET_X_LPARAM(lParam);
    TempPt.y = GET_Y_LPARAM(lParam);
    GetWindowTextA(pApp->hEditTimer, TimeStr, sizeof(TimeStr));
    pApp->FloodTimer = atoi(TimeStr);
    pApp->bFlagAvtivate ^= 1; /*���� ���� ����Ī*/
    if (pApp->bFlagAvtivate) { /*Ȱ��ȭ ���� ������ ������ ���� �� ��.*/
        /*pApp->Mbps = 1;*/ /*�̹� ������ .Mbps ���� �ٷ� �̿��Ѵ�.*/
        pApp->RunningTime = 0;
        pApp->BaseTick = (unsigned long)GetTickCount64();
        pApp->CurrentSendPackets = 0;
        for (i = 0; i < MAX_THREAD; i++) {
            thArg[i].Index = i;
            thArg[i].pApp = pApp;
            _beginthreadex(0, 0, Thread_Flood, &thArg[i], 0, &thID[i]);
        }
    }
    return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
    APP*            pApp;
    HDC             hDC, MemDC;
    HBITMAP         hBit, hOldBit;
    PAINTSTRUCT     ps;
    RECT            TempRect;
    char            TempStr[1024];
    SIZE            ObjectSize;
    POINT           TempPt;
    int             i, TempInteger, Res;
    unsigned char   rv,gv;
    float           vector;
    char            ThrottleMessage[64];
    char            EtcMessage[64];
    unsigned long   Tick, RemainTick;
    unsigned int    colorR;

    switch (Message) {
    case WM_CREATE:
        pApp = (APP*)calloc(1, sizeof(APP));
        if (!pApp) {PostQuitMessage(0); break;}
        Res = WSAStartup(MAKEWORD(2,2), &pApp->wsa);
        pApp->hWndMain   = hWnd;
        pApp->hEditIP    = CreateWindowExA(NULL, "edit", "192.168.0.1", WS_CHILD | WS_VISIBLE, 10, 10, 120, 15, hWnd, NULL, NULL, NULL);
        pApp->hEditPort  = CreateWindowExA(NULL, "edit", "135", WS_CHILD | WS_VISIBLE, 140, 10, 50, 15, hWnd, NULL, NULL, NULL);
        pApp->hEditTimer = CreateWindowExA(NULL, "edit", "60", WS_CHILD | WS_VISIBLE, 220, 70, 25, 15, hWnd, NULL, NULL, NULL);
        pApp->DefFont    = CreateFontA(16,0,0,0,FW_BOLD,0,0,0, HANGEUL_CHARSET,0,0,0,FF_ROMAN,"���� ���");
        pApp->SmallFont  = CreateFontA(14,0,0,0, FW_MEDIUM,0,0,0, HANGEUL_CHARSET,0,0,0,FF_ROMAN,"���� ���");
        TempInteger = 0;
        for(i=0 ; i<MAX_THREAD ; i++) TempInteger += (int)strlen(DummyLog[i]);
        pApp->AvgDatagramSize = TempInteger/MAX_THREAD;
        pApp->Mbps = 1; /*1 Mbps �̸��� ����.*/
        pApp->BaseTick = (unsigned long)GetTickCount64();
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pApp);
        SendMessage(pApp->hEditIP, WM_SETFONT, (WPARAM)pApp->DefFont, 0) ;
        SendMessage(pApp->hEditPort, WM_SETFONT, (WPARAM)pApp->DefFont, 0);
        SendMessage(pApp->hEditTimer, WM_SETFONT, (WPARAM)pApp->DefFont, 0);
        InitializeCriticalSection(&pApp->cs);
        SetTimer(hWnd, 666, 1, NULL); /*Ÿ�̸� �ɾ�ΰ� �ֱ����� InvalidateRect ȣ��*/
        return 0;

    case WM_CTLCOLOREDIT: /*����Ʈ ���� ���� �޼����� �θ� �����찡 �ް� ��*/
        pApp = (APP*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
        if ((HWND)lParam == pApp->hEditIP || (HWND)lParam == pApp->hEditPort || (HWND)lParam == pApp->hEditTimer){
            SetTextColor((HDC)wParam, RGB(0xe7, 0xe7, 0xe7));
            SetBkColor((HDC)wParam, RGB(0x40, 0x40, 0x40));
            return 0;
        }
        break;

    case WM_MOUSEWHEEL:
    pApp = (APP*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
        if ( GET_WHEEL_DELTA_WPARAM(wParam) > 0){ /*��Ÿ�� ����̸� ��ũ�� ���� �ø� �� (����Ʋ ����)*/
            if ( pApp->Mbps + MBPS_UNIT > MAX_TRAFFIC ) pApp->Mbps = MAX_TRAFFIC; /* ���� ���� ���� */
            else                                        pApp->Mbps += MBPS_UNIT;
        }

        else{ /* �����̸� ��ũ�� �Ʒ��� ���� �� (����Ʋ ����)*/
            if ( pApp->Mbps - MBPS_UNIT < 1 ) pApp->Mbps = 1;          /* �ּ� ���� ���� ���� */
            else                              pApp->Mbps -= MBPS_UNIT; /* ���� ���� ���� */
        }
            break;
        break;

    case WM_LBUTTONUP:
        TempRect = ACTBUTTON_RECT; TempRect.right+=TempRect.left; TempRect.bottom+=TempRect.top;
        TempPt.x = GET_X_LPARAM(lParam);
        TempPt.y = GET_Y_LPARAM(lParam);
        if (PtInRect(&TempRect, TempPt)) /*��ư Ŭ������ ����*/
            OnActivateButtonClick(hWnd, Message, wParam, lParam);

        return 0;

    case WM_TIMER:
        InvalidateRect(hWnd, NULL, FALSE);
        return 0;

        #if 1
    case WM_PAINT:
        pApp = (APP*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

        hDC = BeginPaint(hWnd, &ps);
        MemDC = CreateCompatibleDC(hDC);
        hBit = CreateCompatibleBitmap(hDC, WIN_X, WIN_Y);
        hOldBit = (HBITMAP)SelectObject(MemDC, hBit);
        SetBkMode(MemDC, TRANSPARENT);
        /*���ķδ� MemDC�� �׸��� ����*/
        Draw_Rectangle(MemDC, 0, 0, WIN_X, WIN_Y, NULL, RGB(25,25,25), TRUE, FALSE);

        /*������ư*/
        TempRect = ACTBUTTON_RECT;
        if (pApp->bFlagAvtivate){
            Tick = distance(pApp->BaseTick, (unsigned long)GetTickCount64());
            colorR = 0x40 + (unsigned char)( 0x5f * ((sin(Tick/200.f))+1.f) / 2.f );
            Draw_Rectangle(MemDC, TempRect.left, TempRect.top, TempRect.right, TempRect.bottom, 0, RGB(colorR, 0x38, 0x38), TRUE, FALSE);
            strcpy(TempStr, "Flooding");
        }
        else {
            Draw_Rectangle(MemDC, TempRect.left, TempRect.top, TempRect.right, TempRect.bottom, 0, RGB(0x30, 0x60, 0x30),TRUE, FALSE);
            strcpy(TempStr, "Start");
        }
        Draw_GetTextExtent(MemDC, TempStr, pApp->DefFont, &ObjectSize); /*�ؽ�Ʈ�� ��Ʈ��� ũ�� ���*/
        Draw_Text(MemDC, TempStr,
                  (TempRect.right >> 1) - (ObjectSize.cx >> 1) + TempRect.left,
                  (TempRect.bottom >> 1) - (ObjectSize.cy >> 1) + TempRect.top - 2,
                  RGB(230, 230, 230), pApp->DefFont);
        
        /*����Ʋ ��*/
        TempRect = THROTTLE_RECT;
        Draw_Rectangle(MemDC, TempRect.left, TempRect.top, TempRect.right, TempRect.bottom, 0, RGB(0x27, 0x27, 0x27),TRUE, FALSE);
        
        /*����Ʋ ���ؿ����� ����� ��� ũ�� ����. (���콺 �ٷ� ������)*/
        vector = pApp->Mbps /(float)MAX_TRAFFIC;
        rv = 0x4f; rv = (unsigned char)(rv * vector);                 /*���Ͱ� ��������(����Ʋ�� ��������) rv ������ 0.0�� ����*/
        gv = 0x4f; gv = (unsigned char)(gv * vector); gv = 0x4f - gv; /*���Ͱ� ��������(����Ʋ�� ��������) gv ������ 1.0�� ����*/
        Draw_Rectangle(MemDC, TempRect.left, TempRect.top, (int)(TempRect.right*vector), TempRect.bottom, 0, RGB(0x27+rv, 0x27+gv, 0x27),TRUE, FALSE);

        sprintf(ThrottleMessage, "Throttle : %d Mbps", pApp->Mbps);
        Draw_GetTextExtent(MemDC, ThrottleMessage, pApp->DefFont, &ObjectSize); /*�ؽ�Ʈ�� ��Ʈ��� ũ�� ���*/
        Draw_Text(MemDC, ThrottleMessage,
                  (TempRect.right >> 1) - (ObjectSize.cx >> 1) + TempRect.left,
                  (TempRect.bottom >> 1) - (ObjectSize.cy >> 1) + TempRect.top - 2,
                  RGB(230, 230, 230), pApp->DefFont);

        /*��Ÿ �޼��� ���*/
        sprintf(EtcMessage, "Avg DGram Size : %d Bytes", pApp->AvgDatagramSize);
        Draw_Text(MemDC, EtcMessage, 10, 72, RGB(230,230,230), pApp->SmallFont);
        Draw_Text(MemDC, (char*)"Timer:", 180, 70, RGB(230,230,230), pApp->DefFont);
        Draw_Text(MemDC, (char*)"Sec", 250, 70, RGB(230,230,230), pApp->DefFont);

        if (pApp->bFlagAvtivate) {
            Tick = distance(pApp->BaseTick, (unsigned long)GetTickCount64()); /*���� �÷��� ����Ÿ�� ���ϱ�*/
            pApp->CurrentPPS = (int)(pApp->CurrentSendPackets / (Tick / 1000.f));
        }
        sprintf(EtcMessage, "Current PPS : %d Pkt ( %d Pkt/s )", pApp->CurrentSendPackets, pApp->CurrentPPS);
        Draw_Text(MemDC, EtcMessage, 10, 90, RGB(230,230,230), pApp->SmallFont);

        /*�÷��� Ÿ�̸Ӱ� �����Ǿ��ִٸ� (Ÿ�̸Ӱ� 0�� �ƴ϶��)*/
        if (pApp->bFlagAvtivate && pApp->FloodTimer){
            RemainTick = pApp->FloodTimer*1000 - Tick; /*���� ƽ*/
            if (RemainTick <= 0)
                pApp->bFlagAvtivate = FALSE; /*�ð� �Ǹ� ��������*/
            vector = RemainTick / (float)(pApp->FloodTimer*1000);
            TempRect = TIMER_RECT;
            Draw_Rectangle(MemDC, TempRect.left, TempRect.top, (int)(TempRect.right * vector), TempRect.bottom, 0, RGB(0xA0,0xA0,0xA0), TRUE, FALSE);
        }

        /*�׸��� ��*/
        BitBlt(hDC, 0, 0, WIN_X, WIN_Y, MemDC, 0, 0, SRCCOPY);
        SelectObject(MemDC, hOldBit);
        DeleteObject(hBit);
        ReleaseDC(hWnd, MemDC);
        DeleteDC(MemDC);
        ReleaseDC(hWnd, hDC);
        DeleteDC(hDC);
        EndPaint(hWnd, &ps);
        break;
        #endif

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    }
    return DefWindowProc(hWnd, Message, wParam, lParam);
}

int __stdcall WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmdLine, int nShowCmd)
{
    WNDCLASS wc;
    HWND hWnd;
    MSG Message;

    wc.hInstance = hInst;
    wc.lpszClassName = CLASSNAME;
    wc.lpfnWndProc = WndProc;
    wc.hbrBackground = CreateSolidBrush(RGB(0x27, 0x27, 0x27));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpszMenuName = NULL;
    wc.cbClsExtra = NULL;
    wc.cbWndExtra = NULL;
    RegisterClass(&wc);
    /*WS_CLIPSIBLINGS | WS_CLIPCHILDREN �ɼ� �߰��ؼ� InvalidateRect ��, ���ϵ� ��Ʈ�ѱ��� ���� �������� �ʵ��� ����*/
    hWnd = CreateWindow(CLASSNAME, CLASSNAME, WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_SYSMENU | WS_BORDER, 0, 0, WIN_X, WIN_Y, NULL, NULL, hInst, NULL);
    ShowWindow(hWnd, TRUE);
    while (GetMessage(&Message, NULL, NULL, NULL)) {
        TranslateMessage(&Message);
        DispatchMessage(&Message);
    }
    return (int)Message.wParam;
}
