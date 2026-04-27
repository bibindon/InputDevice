#pragma comment( lib, "d3d9.lib" )
#if defined(DEBUG) || defined(_DEBUG)
#pragma comment( lib, "d3dx9d.lib" )
#else
#pragma comment( lib, "d3dx9.lib" )
#endif

#include <d3d9.h>
#include <d3dx9.h>
#include <string>
#include <tchar.h>
#include <cassert>
#include <crtdbg.h>
#include <vector>
#include <windows.h>
#include <mmsystem.h>

#include "../InputDevice/InputDevice.h"
using namespace InputDevice;

#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p) = NULL; } }
#pragma comment(lib, "winmm.lib")

const int WINDOW_SIZE_W = 1600;
const int WINDOW_SIZE_H = 900;

LPDIRECT3D9 g_pD3D = NULL;
LPDIRECT3DDEVICE9 g_pd3dDevice = NULL;
LPD3DXFONT g_pFont = NULL;
LPD3DXMESH g_pMesh = NULL;
std::vector<D3DMATERIAL9> g_pMaterials;
std::vector<LPDIRECT3DTEXTURE9> g_pTextures;
DWORD g_dwNumMaterials = 0;
LPD3DXEFFECT g_pEffect = NULL;
bool g_bClose = false;

static void TextDraw(LPD3DXFONT pFont, TCHAR* text, int X, int Y, D3DCOLOR color = D3DCOLOR_ARGB(255, 0, 0, 0));
static void DrawInputStatus();
static void SetGamePadButtonStatus(GamePadButton button, TCHAR* status, std::size_t statusSize, D3DCOLOR* color);
static std::wstring KeyCodeToString(int keyCode);
static float GetFps();
static void InitD3D(HWND hWnd);
static void Cleanup();
static void Render();
LRESULT WINAPI MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

extern int WINAPI _tWinMain(_In_ HINSTANCE hInstance,
                            _In_opt_ HINSTANCE hPrevInstance,
                            _In_ LPTSTR lpCmdLine,
                            _In_ int nCmdShow);

int WINAPI _tWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPTSTR lpCmdLine,
                     _In_ int nCmdShow)
{
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    timeBeginPeriod(1);

    WNDCLASSEX wc { };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = MsgProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hIcon = NULL;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszMenuName = NULL;
    wc.lpszClassName = _T("Window1");
    wc.hIconSm = NULL;

    ATOM atom = RegisterClassEx(&wc);
    assert(atom != 0);

    RECT rect;
    SetRect(&rect, 0, 0, WINDOW_SIZE_W, WINDOW_SIZE_H);
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    rect.right = rect.right - rect.left;
    rect.bottom = rect.bottom - rect.top;
    rect.top = 0;
    rect.left = 0;

    HWND hWnd = CreateWindow(_T("Window1"),
                             _T("Hello DirectX9 World !!"),
                             WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT,
                             CW_USEDEFAULT,
                             rect.right,
                             rect.bottom,
                             NULL,
                             NULL,
                             wc.hInstance,
                             NULL);

    ShowWindow(hWnd, SW_SHOWDEFAULT);
    UpdateWindow(hWnd);

    InitD3D(hWnd);
    Initialize(hInstance, hWnd);

    MSG msg;

    while (true)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                break;
            }

            DispatchMessage(&msg);
        }
        else
        {
            Sleep(16);

            Update();
            Render();
        }

        if (g_bClose)
        {
            break;
        }
    }

    Finalize();
    Cleanup();
    timeEndPeriod(1);

    UnregisterClass(_T("Window1"), wc.hInstance);
    return 0;
}

void TextDraw(LPD3DXFONT pFont, TCHAR* text, int X, int Y, D3DCOLOR color)
{
    RECT rect = { X, Y, 0, 0 };

    // DrawTextの戻り値は文字数である。
    // そのため、hResultの中身が整数でもエラーが起きているわけではない。
    HRESULT hResult = pFont->DrawText(NULL,
                                      text,
                                      -1,
                                      &rect,
                                      DT_LEFT | DT_NOCLIP,
                                      color);

    assert((int)hResult >= 0);
}

void DrawInputStatus()
{
    if (SKeyBoard::IsDownFirstFrame(DIK_F1))
    {
        Mouse::SetVisible(!Mouse::IsVisible());
    }

    TCHAR msg[256];
    TCHAR leftMouseStatus[32];
    TCHAR rightMouseStatus[32];
    TCHAR middleMouseStatus[32];
    const bool isMouseInWindow = Mouse::IsInWindow();
    const bool isMouseVisible = Mouse::IsVisible();
    const TCHAR* cursorVisibleText = _T("Hidden");
    const TCHAR* mouseInWindowText = _T("Out");
    MousePosition mousePosition = Mouse::GetPosition();
    GamePadStick gamePadStick;
    MousePosition mouseDelta = Mouse::GetDelta(&gamePadStick);
    TCHAR gamePadUpStatus[32];
    TCHAR gamePadRightStatus[32];
    TCHAR gamePadDownStatus[32];
    TCHAR gamePadLeftStatus[32];
    TCHAR gamePadAStatus[32];
    TCHAR gamePadBStatus[32];
    TCHAR gamePadXStatus[32];
    TCHAR gamePadYStatus[32];
    TCHAR gamePadStartStatus[32];
    TCHAR gamePadBackStatus[32];
    TCHAR gamePadR1Status[32];
    TCHAR gamePadR2Status[32];
    TCHAR gamePadL1Status[32];
    TCHAR gamePadL2Status[32];
    GamePadStick stickL = GamePad::GetStickL();
    GamePadStick stickR = GamePad::GetStickR();
    D3DCOLOR keyboardColor = D3DCOLOR_ARGB(255, 0, 0, 0);
    D3DCOLOR mouseColor = D3DCOLOR_ARGB(255, 0, 0, 0);
    D3DCOLOR gamePadColor = D3DCOLOR_ARGB(255, 0, 0, 0);
    std::wstring keyboardText;
    bool hasKeyboardInput = false;

    _stprintf_s(msg, 256, _T("FPS: %.2f"), GetFps());
    TextDraw(g_pFont, msg, 20, 20);

    _tcscpy_s(msg, 256, _T("Keyboard Input"));
    TextDraw(g_pFont, msg, 20, 60);

    for (int keyCode = 0; keyCode < 256; ++keyCode)
    {
        if (!SKeyBoard::IsDown(keyCode))
        {
            continue;
        }

        if (hasKeyboardInput)
        {
            keyboardText += L", ";
        }

        keyboardText += KeyCodeToString(keyCode);
        keyboardText += L"(";
        keyboardText += L"Down";

        if (SKeyBoard::IsHold(keyCode))
        {
            keyboardText += L"+Hold";
        }
        else if (SKeyBoard::IsDownFirstFrame(keyCode))
        {
            keyboardText += L"+First";
            keyboardColor = D3DCOLOR_ARGB(255, 0, 160, 0);
        }
        keyboardText += L")";

        hasKeyboardInput = true;
    }

    if (!hasKeyboardInput)
    {
        keyboardText = L"None";
    }

    _snwprintf_s(msg, 256, _TRUNCATE, L"%s", keyboardText.c_str());
    TextDraw(g_pFont, msg, 20, 90, keyboardColor);

    if (isMouseVisible)
    {
        cursorVisibleText = _T("Visible");
    }

    if (isMouseInWindow)
    {
        mouseInWindowText = _T("In");
    }

    _stprintf_s(msg,
                256,
                _T("F1: Cursor Show/Hide  Current:%s"),
                cursorVisibleText);
    TextDraw(g_pFont, msg, 20, 115);

    _stprintf_s(msg,
                256,
                _T("Mouse: Left / Right / Middle  x:%ld  y:%ld  %s"),
                mousePosition.x,
                mousePosition.y,
                mouseInWindowText);
    TextDraw(g_pFont, msg, 20, 140);

    _stprintf_s(msg, 256, _T("Mouse Delta: x:%ld  y:%ld stick x:%.2f stick y:%.2f stick power:%.2f, stick angle:%.2f"),
                mouseDelta.x,
                mouseDelta.y,
                gamePadStick.x,
                gamePadStick.y,
                gamePadStick.power,
                gamePadStick.angle );
    TextDraw(g_pFont, msg, 20, 160);

    if (Mouse::IsDown(0))
    {
        if (Mouse::IsHold(0))
        {
            _tcscpy_s(leftMouseStatus, 32, _T("Down+Hold"));
        }
        else if (Mouse::IsDownFirstFrame(0))
        {
            _tcscpy_s(leftMouseStatus, 32, _T("Down+First"));
            mouseColor = D3DCOLOR_ARGB(255, 0, 160, 0);
        }
        else
        {
            _tcscpy_s(leftMouseStatus, 32, _T("Down"));
        }
    }
    else
    {
        _tcscpy_s(leftMouseStatus, 32, _T("Up"));
    }

    if (Mouse::IsDown(1))
    {
        if (Mouse::IsHold(1))
        {
            _tcscpy_s(rightMouseStatus, 32, _T("Down+Hold"));
        }
        else if (Mouse::IsDownFirstFrame(1))
        {
            _tcscpy_s(rightMouseStatus, 32, _T("Down+First"));
            mouseColor = D3DCOLOR_ARGB(255, 0, 160, 0);
        }
        else
        {
            _tcscpy_s(rightMouseStatus, 32, _T("Down"));
        }
    }
    else
    {
        _tcscpy_s(rightMouseStatus, 32, _T("Up"));
    }

    if (Mouse::IsDown(2))
    {
        if (Mouse::IsHold(2))
        {
            _tcscpy_s(middleMouseStatus, 32, _T("Down+Hold"));
        }
        else if (Mouse::IsDownFirstFrame(2))
        {
            _tcscpy_s(middleMouseStatus, 32, _T("Down+First"));
            mouseColor = D3DCOLOR_ARGB(255, 0, 160, 0);
        }
        else
        {
            _tcscpy_s(middleMouseStatus, 32, _T("Down"));
        }
    }
    else
    {
        _tcscpy_s(middleMouseStatus, 32, _T("Up"));
    }

    _stprintf_s(msg,
                256,
                _T("L:%s  R:%s  M:%s"),
                leftMouseStatus,
                rightMouseStatus,
                middleMouseStatus);
    TextDraw(g_pFont, msg, 20, 180, mouseColor);

    _tcscpy_s(msg, 256, _T("GamePad: D-Pad"));
    TextDraw(g_pFont, msg, 20, 220);

    SetGamePadButtonStatus(GAMEPAD_POV_UP, gamePadUpStatus, 32, &gamePadColor);
    SetGamePadButtonStatus(GAMEPAD_POV_RIGHT, gamePadRightStatus, 32, &gamePadColor);
    SetGamePadButtonStatus(GAMEPAD_POV_DOWN, gamePadDownStatus, 32, &gamePadColor);
    SetGamePadButtonStatus(GAMEPAD_POV_LEFT, gamePadLeftStatus, 32, &gamePadColor);

    _stprintf_s(msg,
                256,
                _T("Up:%s  Right:%s  Down:%s  Left:%s"),
                gamePadUpStatus,
                gamePadRightStatus,
                gamePadDownStatus,
                gamePadLeftStatus);
    TextDraw(g_pFont, msg, 20, 250, gamePadColor);

    _tcscpy_s(msg, 256, _T("GamePad: X / Y / A / B"));
    TextDraw(g_pFont, msg, 20, 300);

    SetGamePadButtonStatus(GAMEPAD_A, gamePadAStatus, 32, &gamePadColor);
    SetGamePadButtonStatus(GAMEPAD_B, gamePadBStatus, 32, &gamePadColor);
    SetGamePadButtonStatus(GAMEPAD_X, gamePadXStatus, 32, &gamePadColor);
    SetGamePadButtonStatus(GAMEPAD_Y, gamePadYStatus, 32, &gamePadColor);

    _stprintf_s(msg,
                256,
                _T("X:%s  Y:%s  A:%s  B:%s"),
                gamePadXStatus,
                gamePadYStatus,
                gamePadAStatus,
                gamePadBStatus);
    TextDraw(g_pFont, msg, 20, 330, gamePadColor);

    _tcscpy_s(msg, 256, _T("GamePad: START / BACK / R1 / R2 / L1 / L2"));
    TextDraw(g_pFont, msg, 20, 380);

    SetGamePadButtonStatus(GAMEPAD_START, gamePadStartStatus, 32, &gamePadColor);
    SetGamePadButtonStatus(GAMEPAD_BACK, gamePadBackStatus, 32, &gamePadColor);
    SetGamePadButtonStatus(GAMEPAD_R1, gamePadR1Status, 32, &gamePadColor);
    SetGamePadButtonStatus(GAMEPAD_R2, gamePadR2Status, 32, &gamePadColor);
    SetGamePadButtonStatus(GAMEPAD_L1, gamePadL1Status, 32, &gamePadColor);
    SetGamePadButtonStatus(GAMEPAD_L2, gamePadL2Status, 32, &gamePadColor);

    _stprintf_s(msg,
                256,
                _T("START:%s  BACK:%s"),
                gamePadStartStatus,
                gamePadBackStatus);
    TextDraw(g_pFont, msg, 20, 410, gamePadColor);

    _stprintf_s(msg,
                256,
                _T("R1:%s  R2:%s  L1:%s  L2:%s"),
                gamePadR1Status,
                gamePadR2Status,
                gamePadL1Status,
                gamePadL2Status);
    TextDraw(g_pFont, msg, 20, 440, gamePadColor);

    _tcscpy_s(msg, 256, _T("GamePad: Stick"));
    TextDraw(g_pFont, msg, 20, 490);

    _stprintf_s(msg,
                256,
                _T("L Stick: x:% .2f  y:% .2f  power:%.2f  angle:% .2f"),
                stickL.x,
                stickL.y,
                stickL.power,
                stickL.angle);
    TextDraw(g_pFont, msg, 20, 520, gamePadColor);

    _stprintf_s(msg,
                256,
                _T("R Stick: x:% .2f  y:% .2f  power:%.2f  angle:% .2f"),
                stickR.x,
                stickR.y,
                stickR.power,
                stickR.angle);
    TextDraw(g_pFont, msg, 20, 550, gamePadColor);

}

void SetGamePadButtonStatus(GamePadButton button, TCHAR* status, std::size_t statusSize, D3DCOLOR* color)
{
    if (GamePad::IsDown(button))
    {
        if (GamePad::IsHold(button))
        {
            _tcscpy_s(status, statusSize, _T("Down+Hold"));
        }
        else if (GamePad::IsDownFirstFrame(button))
        {
            _tcscpy_s(status, statusSize, _T("Down+First"));
            *color = D3DCOLOR_ARGB(255, 0, 160, 0);
        }
        else
        {
            _tcscpy_s(status, statusSize, _T("Down"));
        }
    }
    else
    {
        _tcscpy_s(status, statusSize, _T("Up"));
    }
}

std::wstring KeyCodeToString(int keyCode)
{
    if (DIK_A <= keyCode && keyCode <= DIK_Z)
    {
        wchar_t ch = static_cast<wchar_t>(L'A' + (keyCode - DIK_A));
        return std::wstring(1, ch);
    }

    if (DIK_0 <= keyCode && keyCode <= DIK_9)
    {
        wchar_t ch = static_cast<wchar_t>(L'0' + (keyCode - DIK_0));
        return std::wstring(1, ch);
    }

    switch (keyCode)
    {
    case DIK_SPACE: return L"Space";
    case DIK_RETURN: return L"Enter";
    case DIK_ESCAPE: return L"Escape";
    case DIK_TAB: return L"Tab";
    case DIK_BACK: return L"Backspace";
    case DIK_LSHIFT: return L"LShift";
    case DIK_RSHIFT: return L"RShift";
    case DIK_LCONTROL: return L"LCtrl";
    case DIK_RCONTROL: return L"RCtrl";
    case DIK_LALT: return L"LAlt";
    case DIK_RALT: return L"RAlt";
    case DIK_UP: return L"Up";
    case DIK_DOWN: return L"Down";
    case DIK_LEFT: return L"Left";
    case DIK_RIGHT: return L"Right";
    default:
    {
        wchar_t buffer[32];
        _snwprintf_s(buffer, 32, _TRUNCATE, L"DIK_%d", keyCode);
        return buffer;
    }
    }
}

float GetFps()
{
    static LARGE_INTEGER frequency = { };
    static LARGE_INTEGER lastCounter = { };
    static int frameCount = 0;
    static float fps = 0.0f;

    if (frequency.QuadPart == 0)
    {
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&lastCounter);
    }

    ++frameCount;

    LARGE_INTEGER currentCounter;
    QueryPerformanceCounter(&currentCounter);

    const double elapsedSeconds =
        static_cast<double>(currentCounter.QuadPart - lastCounter.QuadPart) /
        static_cast<double>(frequency.QuadPart);

    if (elapsedSeconds >= 1.0)
    {
        fps = static_cast<float>(frameCount / elapsedSeconds);
        frameCount = 0;
        lastCounter = currentCounter;
    }

    return fps;
}

void InitD3D(HWND hWnd)
{
    HRESULT hResult = E_FAIL;

    g_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    assert(g_pD3D != NULL);

    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    d3dpp.BackBufferCount = 1;
    d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;
    d3dpp.MultiSampleQuality = 0;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    d3dpp.hDeviceWindow = hWnd;
    d3dpp.Flags = 0;
    d3dpp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
    d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    hResult = g_pD3D->CreateDevice(D3DADAPTER_DEFAULT,
                                   D3DDEVTYPE_HAL,
                                   hWnd,
                                   D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                   &d3dpp,
                                   &g_pd3dDevice);

    if (FAILED(hResult))
    {
        hResult = g_pD3D->CreateDevice(D3DADAPTER_DEFAULT,
                                       D3DDEVTYPE_HAL,
                                       hWnd,
                                       D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                       &d3dpp,
                                       &g_pd3dDevice);

        assert(hResult == S_OK);
    }

    hResult = D3DXCreateFont(g_pd3dDevice,
                             20,
                             0,
                             FW_HEAVY,
                             1,
                             FALSE,
                             SHIFTJIS_CHARSET,
                             OUT_TT_ONLY_PRECIS,
                             CLEARTYPE_NATURAL_QUALITY,
                             FF_DONTCARE,
                             _T("ＭＳ ゴシック"),
                             &g_pFont);

    assert(hResult == S_OK);

    LPD3DXBUFFER pD3DXMtrlBuffer = NULL;

    hResult = D3DXLoadMeshFromX(_T("cube.x"),
                                D3DXMESH_SYSTEMMEM,
                                g_pd3dDevice,
                                NULL,
                                &pD3DXMtrlBuffer,
                                NULL,
                                &g_dwNumMaterials,
                                &g_pMesh);

    assert(hResult == S_OK);

    D3DXMATERIAL* d3dxMaterials = (D3DXMATERIAL*)pD3DXMtrlBuffer->GetBufferPointer();
    g_pMaterials.resize(g_dwNumMaterials);
    g_pTextures.resize(g_dwNumMaterials);

    for (DWORD i = 0; i < g_dwNumMaterials; i++)
    {
        g_pMaterials[i] = d3dxMaterials[i].MatD3D;
        g_pMaterials[i].Ambient = g_pMaterials[i].Diffuse;
        g_pTextures[i] = NULL;
        
        //--------------------------------------------------------------
        // Unicode文字セットでもマルチバイト文字セットでも
        // "d3dxMaterials[i].pTextureFilename"はマルチバイト文字セットになる。
        // 
        // 一方で、D3DXCreateTextureFromFileはプロジェクト設定で
        // Unicode文字セットかマルチバイト文字セットか変わる。
        //--------------------------------------------------------------

        std::string pTexPath(d3dxMaterials[i].pTextureFilename);

        if (!pTexPath.empty())
        {
            bool bUnicode = false;

#ifdef UNICODE
            bUnicode = true;
#endif

            if (!bUnicode)
            {
                hResult = D3DXCreateTextureFromFileA(g_pd3dDevice, pTexPath.c_str(), &g_pTextures[i]);
                assert(hResult == S_OK);
            }
            else
            {
                int len = MultiByteToWideChar(CP_ACP, 0, pTexPath.c_str(), -1, nullptr, 0);
                std::wstring pTexPathW(len, 0);
                MultiByteToWideChar(CP_ACP, 0, pTexPath.c_str(), -1, &pTexPathW[0], len);

                hResult = D3DXCreateTextureFromFileW(g_pd3dDevice, pTexPathW.c_str(), &g_pTextures[i]);
                assert(hResult == S_OK);
            }
        }
    }

    hResult = pD3DXMtrlBuffer->Release();
    assert(hResult == S_OK);

    hResult = D3DXCreateEffectFromFile(g_pd3dDevice,
                                       _T("simple.fx"),
                                       NULL,
                                       NULL,
                                       D3DXSHADER_DEBUG,
                                       NULL,
                                       &g_pEffect,
                                       NULL);

    assert(hResult == S_OK);
}

void Cleanup()
{
    if (g_pd3dDevice != NULL)
    {
        for (DWORD i = 0; i < 8; ++i)
        {
            g_pd3dDevice->SetTexture(i, NULL);
        }

        g_pd3dDevice->SetStreamSource(0, NULL, 0, 0);
        g_pd3dDevice->SetIndices(NULL);
        g_pd3dDevice->SetVertexShader(NULL);
        g_pd3dDevice->SetPixelShader(NULL);
        g_pd3dDevice->SetVertexDeclaration(NULL);
    }

    if (g_pEffect != NULL)
    {
        g_pEffect->SetTexture("texture1", NULL);
        g_pEffect->OnLostDevice();
    }

    if (g_pFont != NULL)
    {
        g_pFont->OnLostDevice();
    }

    SAFE_RELEASE(g_pEffect);
    SAFE_RELEASE(g_pFont);
    SAFE_RELEASE(g_pMesh);

    for (auto& texture : g_pTextures)
    {
        SAFE_RELEASE(texture);
    }

    std::vector<LPDIRECT3DTEXTURE9> emptyTextures;
    g_pTextures.swap(emptyTextures);

    std::vector<D3DMATERIAL9> emptyMaterials;
    g_pMaterials.swap(emptyMaterials);

    g_dwNumMaterials = 0;

    SAFE_RELEASE(g_pd3dDevice);
    SAFE_RELEASE(g_pD3D);
}

void Render()
{
    HRESULT hResult = E_FAIL;

    static float f = 0.0f;
    f += 0.025f;

    D3DXMATRIX mat;
    D3DXMATRIX View, Proj;

    D3DXMatrixPerspectiveFovLH(&Proj,
                               D3DXToRadian(45),
                               (float)WINDOW_SIZE_W / WINDOW_SIZE_H,
                               1.0f,
                               10000.0f);

    D3DXVECTOR3 vec1(10 * sinf(f), 10, -10 * cosf(f));
    D3DXVECTOR3 vec2(0, 0, 0);
    D3DXVECTOR3 vec3(0, 1, 0);
    D3DXMatrixLookAtLH(&View, &vec1, &vec2, &vec3);
    D3DXMatrixIdentity(&mat);
    mat = mat * View * Proj;

    hResult = g_pEffect->SetMatrix("g_matWorldViewProj", &mat);
    assert(hResult == S_OK);

    hResult = g_pd3dDevice->Clear(0,
                                  NULL,
                                  D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                                  D3DCOLOR_XRGB(100, 100, 100),
                                  1.0f,
                                  0);

    assert(hResult == S_OK);

    hResult = g_pd3dDevice->BeginScene();
    assert(hResult == S_OK);

    DrawInputStatus();

    hResult = g_pEffect->SetTechnique("Technique1");
    assert(hResult == S_OK);

    UINT numPass;
    hResult = g_pEffect->Begin(&numPass, 0);
    assert(hResult == S_OK);

    hResult = g_pEffect->BeginPass(0);
    assert(hResult == S_OK);

    for (DWORD i = 0; i < g_dwNumMaterials; i++)
    {
        hResult = g_pEffect->SetTexture("texture1", g_pTextures[i]);
        assert(hResult == S_OK);

        hResult = g_pEffect->CommitChanges();
        assert(hResult == S_OK);

        hResult = g_pMesh->DrawSubset(i);
        assert(hResult == S_OK);
    }

    hResult = g_pEffect->EndPass();
    assert(hResult == S_OK);

    hResult = g_pEffect->End();
    assert(hResult == S_OK);

    hResult = g_pd3dDevice->EndScene();
    assert(hResult == S_OK);

    hResult = g_pd3dDevice->Present(NULL, NULL, NULL, NULL);
    assert(hResult == S_OK);
}

LRESULT WINAPI MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY:
    {
        PostQuitMessage(0);
        g_bClose = true;
        return 0;
    }
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

