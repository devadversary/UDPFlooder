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

#define ACTBUTTON_RECT {200,10,75,16}   /* 시작/스답버튼 좌표와 크기 */
#define THROTTLE_RECT  {10,35,265,30}   /* 스로틀 바 좌표와 크기 */
#define TIMER_RECT     {10,62,265,3}    /* 타이머 바 좌표와 크기 */
#define MAX_TRAFFIC    (100)            /* 트래픽 한계치 (단위 Mbps) (12500000 바이트) */
#define MBPS_UNIT      (3)              /* 트래픽 조절 단위 (3Mbps 단위로 조절)*/
#define MAX_THREAD     (4)              /* 생성할 스레드 갯수 */

typedef struct _st_APP_
{
    WSADATA          wsa;                /**< 윈속 라이브러리 핸들*/
    HWND             hWndMain;           /**< 메인윈도우*/
    WNDPROC          fnMainProc;         /**< 메인 윈도우 프로시저*/
    HFONT            DefFont;            /**< 사용할 기본 폰트*/
    HFONT            SmallFont;          /**< 살짝 작은 폰트*/
    HWND             hEditIP;            /**< IP 입력 에디트 핸들*/
    HWND             hEditPort;          /**< Port 입력 에디트 핸들*/
    HWND             hEditTimer;         /**< 타이버 에디트 핸들*/

    BOOL             bFlagAvtivate;      /**< 트래픽 부하 구동여부. (TRUE이면 켜진것)*/
    CRITICAL_SECTION cs;                 /**< 스레드 동기화 객체*/

    SOCKADDR_IN      TargetInfo;         /**< 테스트 대상 주소정보*/
    int              Mbps;               /**< 현재 트래픽 스로틀 (0~300Mbps)*/
    int              AvgDatagramSize;    /**< 평균 UDP 데이터그램 크기*/
    int              CurrentSendPackets; /**< 보낸 패킷수*/
    int              CurrentPPS;         /**< 1초당 패킷수*/
    unsigned long    RunningTime;        /**< 테스트 시간 (ms)*/
    unsigned long    BaseTick;           /**< 테스트 시간을 구하기위한 기준 틱*/
    int              FloodTimer;         /**< 플러딩 지속 시간*/

} APP;

/**
    @brief 플러딩 스레드 인자로 넘길 구조체
*/ 
typedef struct _st_threadarg_flood
{
    APP* pApp;  /* 이 프로그램의 전역 핸들 */
    int  Index; /* 이 스레드의 ID 값. (어떤 더미 문자열을 플러딩 할 것인지 이 값으로 결정.)*/
} THREADARG_FLOOD;

/**
   @brief UDP로 보낼 더미데이터 (가상의 Syslog 데이터이다)
*/
char DummyLog[4][512] = {"Nov  4 10:35:05 deava666 whoopsie[1179]: [10:35:05] Not a paid data plan: /org/freedesktop/NetworkManager/ActiveConnection/1",
                         "Nov  4 11:00:24 Nova666 dbus-daemon[870]: [system] Activating via systemd: service name='org.freedesktop.nm_dispatcher' unit='dbus-org.freedesktop.nm-dispatcher.service' requested by ':1.12' (uid=0 pid=871 comm=\" / usr / sbin / NetworkManager --no - daemon \" label=\"unconfined\")",
                         "Nov  4 05:26:05 Lava666 NetworkManager[871]: <info>  [1635971165.1844] manager: NetworkManager state is now CONNECTED_GLOBAL",
                         "Nov  4 04:29:20 Java666 dbus-daemon[870]: [system] Activating via systemd: service name='org.freedesktop.fwupd' unit='fwupd.service' requested by ':1.96540' (uid=62803 pid=2796143 comm=\"/usr/bin/fwupdmgr refresh\"label=\"unconfined\")"};

/**
    @brief UDP 부하용 스레드
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

    Index %= MAX_THREAD; /*스레드 갯수를 넘어가는 인덱스는 적절히 잘라준다.*/
    sock = socket(AF_INET, SOCK_DGRAM, NULL);
    if (!sock) exit(-1);

    memcpy(&targetadr, &pApp->TargetInfo, sizeof(SOCKADDR));
    nLenStr = (int)strlen(DummyLog[Index]);
    pDummy = DummyLog[Index];
    TotalSend = 0;
    BytePerSec = 0;
    Shutter_Init(&Shutter, 1000, TRUE); /*1초 마다 체크*/
    while(1){
        if (!pApp->bFlagAvtivate){ /*활성화 되지 않은 상태라면 즉시 스레드 종료*/
            closesocket(sock);
            return 0;
        }

        if (Shutter_Check(&Shutter)){ /*지정 체크 시간 돌아오면 */
            BytePerSec = pApp->Mbps;  /* 지정 시간마다 Mbps 갱신 */
            BytePerSec *= 1048576;    /* 1메가 곱하기 */
            BytePerSec >>= 3;         /* 비트수 8 나누기 */
            BytePerSec /= MAX_THREAD; /* 스레드 수 나누기 */
            TotalSend = 0;            /* 보낸 바이트 수 다시 초기화 */
        }

        if (TotalSend >= BytePerSec ) { 
            Sleep(1); /*잠깐 쉬고*/
            continue; /*패킷 송신 유보*/
        }
        
        Tx = sendto(sock, pDummy, nLenStr, 0, &targetadr, sizeof(SOCKADDR));
        /*이더넷 + IP + TCP/UDP 헤더 길이까지 포함 시킬것인지 선택 사항.*/
        Tx += (14 + 20 + 8); /*이더넷,IP,UDP 헤더길이까지 합산*/
        pApp->CurrentSendPackets ++;
        TotalSend += Tx;
    }
    return 0;
}

/**
    @brief 플러딩 버튼 클릭시 호출될 핸들러
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
        MessageBoxA(hWnd, "유효하지 않은 주소 입니다.", "Error", MB_ICONWARNING);
        return FALSE;
    }
    GetWindowTextA(pApp->hEditPort, PortAddress, sizeof(PortAddress));
    Port = atoi(PortAddress);
    if (Port < 1 || Port > 65535) {
        MessageBoxA(hWnd, "유효하지 않은 포트 입니다.", "Error", MB_ICONWARNING);
        return FALSE;
    }
    pApp->TargetInfo.sin_port = htons(Port);
    pApp->TargetInfo.sin_family = AF_INET;
    TempRect = ACTBUTTON_RECT; TempRect.right += TempRect.left; TempRect.bottom += TempRect.top;
    TempPt.x = GET_X_LPARAM(lParam);
    TempPt.y = GET_Y_LPARAM(lParam);
    GetWindowTextA(pApp->hEditTimer, TimeStr, sizeof(TimeStr));
    pApp->FloodTimer = atoi(TimeStr);
    pApp->bFlagAvtivate ^= 1; /*동작 상태 스위칭*/
    if (pApp->bFlagAvtivate) { /*활성화 상태 에서만 스레드 생성 해 줌.*/
        /*pApp->Mbps = 1;*/ /*이미 결정된 .Mbps 값을 바로 이용한다.*/
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
        pApp->DefFont    = CreateFontA(16,0,0,0,FW_BOLD,0,0,0, HANGEUL_CHARSET,0,0,0,FF_ROMAN,"맑은 고딕");
        pApp->SmallFont  = CreateFontA(14,0,0,0, FW_MEDIUM,0,0,0, HANGEUL_CHARSET,0,0,0,FF_ROMAN,"맑은 고딕");
        TempInteger = 0;
        for(i=0 ; i<MAX_THREAD ; i++) TempInteger += (int)strlen(DummyLog[i]);
        pApp->AvgDatagramSize = TempInteger/MAX_THREAD;
        pApp->Mbps = 1; /*1 Mbps 미만은 없다.*/
        pApp->BaseTick = (unsigned long)GetTickCount64();
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pApp);
        SendMessage(pApp->hEditIP, WM_SETFONT, (WPARAM)pApp->DefFont, 0) ;
        SendMessage(pApp->hEditPort, WM_SETFONT, (WPARAM)pApp->DefFont, 0);
        SendMessage(pApp->hEditTimer, WM_SETFONT, (WPARAM)pApp->DefFont, 0);
        InitializeCriticalSection(&pApp->cs);
        SetTimer(hWnd, 666, 1, NULL); /*타이머 걸어두고 주기적인 InvalidateRect 호출*/
        return 0;

    case WM_CTLCOLOREDIT: /*에디트 색상 변경 메세지는 부모 윈도우가 받게 됨*/
        pApp = (APP*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
        if ((HWND)lParam == pApp->hEditIP || (HWND)lParam == pApp->hEditPort || (HWND)lParam == pApp->hEditTimer){
            SetTextColor((HDC)wParam, RGB(0xe7, 0xe7, 0xe7));
            SetBkColor((HDC)wParam, RGB(0x40, 0x40, 0x40));
            return 0;
        }
        break;

    case WM_MOUSEWHEEL:
    pApp = (APP*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
        if ( GET_WHEEL_DELTA_WPARAM(wParam) > 0){ /*델타가 양수이면 스크롤 위로 올린 것 (스로틀 증가)*/
            if ( pApp->Mbps + MBPS_UNIT > MAX_TRAFFIC ) pApp->Mbps = MAX_TRAFFIC; /* 단위 부하 증가 */
            else                                        pApp->Mbps += MBPS_UNIT;
        }

        else{ /* 음수이면 스크롤 아래로 내린 것 (스로틀 감소)*/
            if ( pApp->Mbps - MBPS_UNIT < 1 ) pApp->Mbps = 1;          /* 최소 단위 부하 고정 */
            else                              pApp->Mbps -= MBPS_UNIT; /* 단위 부하 감소 */
        }
            break;
        break;

    case WM_LBUTTONUP:
        TempRect = ACTBUTTON_RECT; TempRect.right+=TempRect.left; TempRect.bottom+=TempRect.top;
        TempPt.x = GET_X_LPARAM(lParam);
        TempPt.y = GET_Y_LPARAM(lParam);
        if (PtInRect(&TempRect, TempPt)) /*버튼 클릭여부 조사*/
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
        /*이후로는 MemDC에 그리기 시작*/
        Draw_Rectangle(MemDC, 0, 0, WIN_X, WIN_Y, NULL, RGB(25,25,25), TRUE, FALSE);

        /*구동버튼*/
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
        Draw_GetTextExtent(MemDC, TempStr, pApp->DefFont, &ObjectSize); /*텍스트의 폰트출력 크기 얻기*/
        Draw_Text(MemDC, TempStr,
                  (TempRect.right >> 1) - (ObjectSize.cx >> 1) + TempRect.left,
                  (TempRect.bottom >> 1) - (ObjectSize.cy >> 1) + TempRect.top - 2,
                  RGB(230, 230, 230), pApp->DefFont);
        
        /*스로틀 바*/
        TempRect = THROTTLE_RECT;
        Draw_Rectangle(MemDC, TempRect.left, TempRect.top, TempRect.right, TempRect.bottom, 0, RGB(0x27, 0x27, 0x27),TRUE, FALSE);
        
        /*스로틀 수준에따라 색상과 출력 크기 조절. (마우스 휠로 조절됨)*/
        vector = pApp->Mbps /(float)MAX_TRAFFIC;
        rv = 0x4f; rv = (unsigned char)(rv * vector);                 /*벡터가 낮을수록(스로틀이 낮을수록) rv 색상은 0.0에 근접*/
        gv = 0x4f; gv = (unsigned char)(gv * vector); gv = 0x4f - gv; /*벡터가 낮을수록(스로틀이 낮을수록) gv 색상은 1.0에 근접*/
        Draw_Rectangle(MemDC, TempRect.left, TempRect.top, (int)(TempRect.right*vector), TempRect.bottom, 0, RGB(0x27+rv, 0x27+gv, 0x27),TRUE, FALSE);

        sprintf(ThrottleMessage, "Throttle : %d Mbps", pApp->Mbps);
        Draw_GetTextExtent(MemDC, ThrottleMessage, pApp->DefFont, &ObjectSize); /*텍스트의 폰트출력 크기 얻기*/
        Draw_Text(MemDC, ThrottleMessage,
                  (TempRect.right >> 1) - (ObjectSize.cx >> 1) + TempRect.left,
                  (TempRect.bottom >> 1) - (ObjectSize.cy >> 1) + TempRect.top - 2,
                  RGB(230, 230, 230), pApp->DefFont);

        /*기타 메세지 출력*/
        sprintf(EtcMessage, "Avg DGram Size : %d Bytes", pApp->AvgDatagramSize);
        Draw_Text(MemDC, EtcMessage, 10, 72, RGB(230,230,230), pApp->SmallFont);
        Draw_Text(MemDC, (char*)"Timer:", 180, 70, RGB(230,230,230), pApp->DefFont);
        Draw_Text(MemDC, (char*)"Sec", 250, 70, RGB(230,230,230), pApp->DefFont);

        if (pApp->bFlagAvtivate) {
            Tick = distance(pApp->BaseTick, (unsigned long)GetTickCount64()); /*현재 플러딩 러닝타임 구하기*/
            pApp->CurrentPPS = (int)(pApp->CurrentSendPackets / (Tick / 1000.f));
        }
        sprintf(EtcMessage, "Current PPS : %d Pkt ( %d Pkt/s )", pApp->CurrentSendPackets, pApp->CurrentPPS);
        Draw_Text(MemDC, EtcMessage, 10, 90, RGB(230,230,230), pApp->SmallFont);

        /*플러딩 타이머가 지정되어있다면 (타이머가 0이 아니라면)*/
        if (pApp->bFlagAvtivate && pApp->FloodTimer){
            RemainTick = pApp->FloodTimer*1000 - Tick; /*남은 틱*/
            if (RemainTick <= 0)
                pApp->bFlagAvtivate = FALSE; /*시간 되면 구동종료*/
            vector = RemainTick / (float)(pApp->FloodTimer*1000);
            TempRect = TIMER_RECT;
            Draw_Rectangle(MemDC, TempRect.left, TempRect.top, (int)(TempRect.right * vector), TempRect.bottom, 0, RGB(0xA0,0xA0,0xA0), TRUE, FALSE);
        }

        /*그리기 끝*/
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
    /*WS_CLIPSIBLINGS | WS_CLIPCHILDREN 옵션 추가해서 InvalidateRect 시, 차일드 컨트롤까지 같이 깜빡이지 않도록 방지*/
    hWnd = CreateWindow(CLASSNAME, CLASSNAME, WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_SYSMENU | WS_BORDER, 0, 0, WIN_X, WIN_Y, NULL, NULL, hInst, NULL);
    ShowWindow(hWnd, TRUE);
    while (GetMessage(&Message, NULL, NULL, NULL)) {
        TranslateMessage(&Message);
        DispatchMessage(&Message);
    }
    return (int)Message.wParam;
}
