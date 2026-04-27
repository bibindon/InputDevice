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
#include <cmath>
#include <crtdbg.h>
#include <cstdarg>
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
D3DXVECTOR3 g_cameraTarget(0.0f, 0.0f, 0.0f);
float g_cameraDistance = 12.37f;
float g_cameraYaw = 0.0f;
float g_cameraPitch = 0.0f;

static void TextDraw(LPD3DXFONT pFont, const std::wstring& text, int X, int Y, D3DCOLOR color = D3DCOLOR_ARGB(255, 0, 0, 0));
static void DrawInputStatus();
static std::wstring SetGamePadButtonStatus(GamePadButton button, D3DCOLOR* color);
static std::wstring GetMouseButtonStatus(char button, D3DCOLOR* color);
static std::wstring GetUnifiedInputStatus(GamePadButton button, D3DCOLOR* color);
static std::wstring KeyCodeToString(int keyCode);
static std::wstring FormatText(const wchar_t* format, ...);
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

void TextDraw(LPD3DXFONT pFont, const std::wstring& text, int X, int Y, D3DCOLOR color)
{
    RECT rect = { X, Y, 0, 0 };

    // DrawTextの戻り値は文字数である。
    // そのため、hResultの中身が整数でもエラーが起きているわけではない。
    HRESULT hResult = pFont->DrawText(NULL,
                                      text.c_str(),
                                      -1,
                                      &rect,
                                      DT_LEFT | DT_NOCLIP,
                                      color);

    assert((int)hResult >= 0);
}

std::wstring FormatText(const wchar_t* format, ...)
{
    wchar_t buffer[256];
    va_list args;
    va_start(args, format);
    _vsnwprintf_s(buffer, 256, _TRUNCATE, format, args);
    va_end(args);
    return buffer;
}

void DrawInputStatus()
{
    if (SKeyBoard::IsDownFirstFrame(DIK_F1))
    {
        Mouse::SetVisible(!Mouse::IsVisible());
    }

    const bool isMouseInWindow = Mouse::IsInWindow();
    const bool isMouseVisible = Mouse::IsVisible();
    std::wstring cursorVisibleText = L"Hidden";
    std::wstring mouseInWindowText = L"Out";
    MousePosition mousePosition = Mouse::GetPosition();
    GamePadStick gamePadStick;
    MousePosition mouseDelta = Mouse::GetDelta(&gamePadStick);
    GamePadStick stickL = GamePad::GetStickL();
    GamePadStick stickR = GamePad::GetStickR();
    GamePadStick unifiedStickL = UnifiedInput::GetStickL();
    GamePadStick unifiedStickR = UnifiedInput::GetStickR();
    D3DCOLOR keyboardColor = D3DCOLOR_ARGB(255, 0, 0, 0);
    D3DCOLOR mouseColor = D3DCOLOR_ARGB(255, 0, 0, 0);
    D3DCOLOR gamePadColor = D3DCOLOR_ARGB(255, 0, 0, 0);
    D3DCOLOR unifiedInputColor = D3DCOLOR_ARGB(255, 0, 0, 0);
    std::wstring keyboardText;
    bool hasKeyboardInput = false;

    TextDraw(g_pFont, FormatText(L"FPS: %.2f", GetFps()), 20, 20);

    TextDraw(g_pFont, L"Keyboard Input", 20, 60);

    for (int keyCode = 0; keyCode < 256; ++keyCode)
    {
        const bool isDown = SKeyBoard::IsDown(keyCode);
        const bool isUpFirstFrame = SKeyBoard::IsUpFirstFrame(keyCode);

        if (!isDown && !isUpFirstFrame)
        {
            continue;
        }

        if (hasKeyboardInput)
        {
            keyboardText += L", ";
        }

        keyboardText += KeyCodeToString(keyCode);
        keyboardText += L"(";

        if (isDown)
        {
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
        }
        else if (isUpFirstFrame)
        {
            keyboardText += L"UpFirst";
            keyboardColor = D3DCOLOR_ARGB(255, 200, 0, 0);
        }

        keyboardText += L")";

        hasKeyboardInput = true;
    }

    if (!hasKeyboardInput)
    {
        keyboardText = L"None";
    }

    TextDraw(g_pFont, keyboardText, 20, 90, keyboardColor);

    if (isMouseVisible)
    {
        cursorVisibleText = L"Visible";
    }

    if (isMouseInWindow)
    {
        mouseInWindowText = L"In";
    }

    TextDraw(g_pFont,
             FormatText(L"F1: Cursor Show/Hide  Current:%s", cursorVisibleText.c_str()),
             20,
             115);

    TextDraw(g_pFont,
             FormatText(L"Mouse: Left / Right / Middle  x:%ld  y:%ld  %s",
                        mousePosition.x,
                        mousePosition.y,
                        mouseInWindowText.c_str()),
             20,
             140);

    TextDraw(g_pFont,
             FormatText(L"Mouse Delta: x:%ld  y:%ld stick x:%.2f stick y:%.2f stick power:%.2f, stick angle:%.2f",
                        mouseDelta.x,
                        mouseDelta.y,
                        gamePadStick.x,
                        gamePadStick.y,
                        gamePadStick.power,
                        gamePadStick.angle),
             20,
             160);

    std::wstring leftMouseStatus = GetMouseButtonStatus(0, &mouseColor);
    std::wstring rightMouseStatus = GetMouseButtonStatus(1, &mouseColor);
    std::wstring middleMouseStatus = GetMouseButtonStatus(2, &mouseColor);

    TextDraw(g_pFont,
             FormatText(L"L:%s  R:%s  M:%s",
                        leftMouseStatus.c_str(),
                        rightMouseStatus.c_str(),
                        middleMouseStatus.c_str()),
             20,
             180,
             mouseColor);

    TextDraw(g_pFont, L"GamePad: D-Pad", 20, 220);

    gamePadColor = D3DCOLOR_ARGB(255, 0, 0, 0);
    std::wstring gamePadUpStatus = SetGamePadButtonStatus(GAMEPAD_POV_UP, &gamePadColor);
    std::wstring gamePadRightStatus = SetGamePadButtonStatus(GAMEPAD_POV_RIGHT, &gamePadColor);
    std::wstring gamePadDownStatus = SetGamePadButtonStatus(GAMEPAD_POV_DOWN, &gamePadColor);
    std::wstring gamePadLeftStatus = SetGamePadButtonStatus(GAMEPAD_POV_LEFT, &gamePadColor);

    TextDraw(g_pFont,
             FormatText(L"Up:%s  Right:%s  Down:%s  Left:%s",
                        gamePadUpStatus.c_str(),
                        gamePadRightStatus.c_str(),
                        gamePadDownStatus.c_str(),
                        gamePadLeftStatus.c_str()),
             20,
             250,
             gamePadColor);

    TextDraw(g_pFont, L"GamePad: X / Y / A / B", 20, 300);

    gamePadColor = D3DCOLOR_ARGB(255, 0, 0, 0);
    std::wstring gamePadAStatus = SetGamePadButtonStatus(GAMEPAD_A, &gamePadColor);
    std::wstring gamePadBStatus = SetGamePadButtonStatus(GAMEPAD_B, &gamePadColor);
    std::wstring gamePadXStatus = SetGamePadButtonStatus(GAMEPAD_X, &gamePadColor);
    std::wstring gamePadYStatus = SetGamePadButtonStatus(GAMEPAD_Y, &gamePadColor);

    TextDraw(g_pFont,
             FormatText(L"X:%s  Y:%s  A:%s  B:%s",
                        gamePadXStatus.c_str(),
                        gamePadYStatus.c_str(),
                        gamePadAStatus.c_str(),
                        gamePadBStatus.c_str()),
             20,
             330,
             gamePadColor);

    TextDraw(g_pFont, L"GamePad: START / BACK / R1 / R2 / L1 / L2", 20, 380);

    gamePadColor = D3DCOLOR_ARGB(255, 0, 0, 0);
    std::wstring gamePadStartStatus = SetGamePadButtonStatus(GAMEPAD_START, &gamePadColor);
    std::wstring gamePadBackStatus = SetGamePadButtonStatus(GAMEPAD_BACK, &gamePadColor);
    std::wstring gamePadR1Status = SetGamePadButtonStatus(GAMEPAD_R1, &gamePadColor);
    std::wstring gamePadR2Status = SetGamePadButtonStatus(GAMEPAD_R2, &gamePadColor);
    std::wstring gamePadL1Status = SetGamePadButtonStatus(GAMEPAD_L1, &gamePadColor);
    std::wstring gamePadL2Status = SetGamePadButtonStatus(GAMEPAD_L2, &gamePadColor);

    TextDraw(g_pFont,
             FormatText(L"START:%s  BACK:%s",
                        gamePadStartStatus.c_str(),
                        gamePadBackStatus.c_str()),
             20,
             410,
             gamePadColor);

    TextDraw(g_pFont,
             FormatText(L"R1:%s  R2:%s  L1:%s  L2:%s",
                        gamePadR1Status.c_str(),
                        gamePadR2Status.c_str(),
                        gamePadL1Status.c_str(),
                        gamePadL2Status.c_str()),
             20,
             440,
             gamePadColor);

    TextDraw(g_pFont, L"GamePad: Stick", 20, 490);

    gamePadColor = D3DCOLOR_ARGB(255, 0, 0, 0);
    TextDraw(g_pFont,
             FormatText(L"L Stick: x:% .2f  y:% .2f  power:%.2f  angle:% .2f",
                        stickL.x,
                        stickL.y,
                        stickL.power,
                        stickL.angle),
             20,
             520,
             gamePadColor);

    TextDraw(g_pFont,
             FormatText(L"R Stick: x:% .2f  y:% .2f  power:%.2f  angle:% .2f",
                        stickR.x,
                        stickR.y,
                        stickR.power,
                        stickR.angle),
             20,
             550,
             gamePadColor);

    TextDraw(g_pFont, L"UnifiedInput: D-Pad (Keyboard Arrow / GamePad D-Pad)", 20, 700);

    unifiedInputColor = D3DCOLOR_ARGB(255, 0, 0, 0);
    std::wstring unifiedUpStatus = GetUnifiedInputStatus(GAMEPAD_POV_UP, &unifiedInputColor);
    std::wstring unifiedRightStatus = GetUnifiedInputStatus(GAMEPAD_POV_RIGHT, &unifiedInputColor);
    std::wstring unifiedDownStatus = GetUnifiedInputStatus(GAMEPAD_POV_DOWN, &unifiedInputColor);
    std::wstring unifiedLeftStatus = GetUnifiedInputStatus(GAMEPAD_POV_LEFT, &unifiedInputColor);

    TextDraw(g_pFont,
             FormatText(L"Up:%s  Right:%s  Down:%s  Left:%s",
                        unifiedUpStatus.c_str(),
                        unifiedRightStatus.c_str(),
                        unifiedDownStatus.c_str(),
                        unifiedLeftStatus.c_str()),
             20,
             730,
             unifiedInputColor);

    TextDraw(g_pFont,
             FormatText(L"UnifiedInput StickL: x:% .2f  y:% .2f  power:%.2f  angle:% .2f",
                        unifiedStickL.x,
                        unifiedStickL.y,
                        unifiedStickL.power,
                        unifiedStickL.angle),
             20,
             760,
             unifiedInputColor);

    TextDraw(g_pFont,
             FormatText(L"UnifiedInput StickR: x:% .2f  y:% .2f  power:%.2f  angle:% .2f",
                        unifiedStickR.x,
                        unifiedStickR.y,
                        unifiedStickR.power,
                        unifiedStickR.angle),
             20,
             790,
             unifiedInputColor);

}

std::wstring GetMouseButtonStatus(char button, D3DCOLOR* color)
{
    if (Mouse::IsDown(button))
    {
        if (Mouse::IsHold(button))
        {
            return L"Down+Hold";
        }
        else if (Mouse::IsDownFirstFrame(button))
        {
            if (color != nullptr)
            {
                *color = D3DCOLOR_ARGB(255, 0, 160, 0);
            }

            return L"Down+First";
        }

        return L"Down";
    }

    if (Mouse::IsUpFirstFrame(button))
    {
        if (color != nullptr)
        {
            *color = D3DCOLOR_ARGB(255, 200, 0, 0);
        }

        return L"Up+First";
    }

    return L"Up";
}

std::wstring SetGamePadButtonStatus(GamePadButton button, D3DCOLOR* color)
{
    if (GamePad::IsDown(button))
    {
        if (GamePad::IsHold(button))
        {
            return L"Down+Hold";
        }
        else if (GamePad::IsDownFirstFrame(button))
        {
            if (color != nullptr)
            {
                *color = D3DCOLOR_ARGB(255, 0, 160, 0);
            }

            return L"Down+First";
        }

        return L"Down";
    }

    if (GamePad::IsUpFirstFrame(button))
    {
        if (color != nullptr)
        {
            *color = D3DCOLOR_ARGB(255, 200, 0, 0);
        }

        return L"Up+First";
    }

    return L"Up";
}

std::wstring GetUnifiedInputStatus(GamePadButton button, D3DCOLOR* color)
{
    if (UnifiedInput::IsDown(button))
    {
        if (UnifiedInput::IsHold(button))
        {
            return L"Down+Hold";
        }
        else if (UnifiedInput::IsDownFirstFrame(button))
        {
            if (color != nullptr)
            {
                *color = D3DCOLOR_ARGB(255, 0, 160, 0);
            }

            return L"Down+First";
        }

        return L"Down";
    }

    if (UnifiedInput::IsUpFirstFrame(button))
    {
        if (color != nullptr)
        {
            *color = D3DCOLOR_ARGB(255, 200, 0, 0);
        }

        return L"Up+First";
    }

    return L"Up";
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

    D3DXMATRIX mat;
    D3DXMATRIX View, Proj;
    GamePadStick moveStick = UnifiedInput::GetStickL();
    GamePadStick lookStick = UnifiedInput::GetStickR();

    g_cameraYaw += lookStick.x * 0.05f;
    g_cameraPitch += lookStick.y * 0.03f;

    if (g_cameraPitch > 1.2f)
    {
        g_cameraPitch = 1.2f;
    }
    else if (g_cameraPitch < -1.2f)
    {
        g_cameraPitch = -1.2f;
    }

    D3DXVECTOR3 forward(std::sinf(g_cameraYaw), 0.0f, std::cosf(g_cameraYaw));
    D3DXVECTOR3 right(std::cosf(g_cameraYaw), 0.0f, -std::sinf(g_cameraYaw));
    float moveSpeed = 0.2f;

    g_cameraTarget += forward * (moveStick.y * moveSpeed);
    g_cameraTarget += right * (moveStick.x * moveSpeed);

    D3DXMatrixPerspectiveFovLH(&Proj,
                               D3DXToRadian(45),
                               (float)WINDOW_SIZE_W / WINDOW_SIZE_H,
                               1.0f,
                               10000.0f);

    D3DXVECTOR3 orbitOffset(std::sinf(g_cameraYaw) * std::cosf(g_cameraPitch),
                            std::sinf(g_cameraPitch),
                            std::cosf(g_cameraYaw) * std::cosf(g_cameraPitch));
    D3DXVECTOR3 cameraPosition = g_cameraTarget - (orbitOffset * g_cameraDistance);
    D3DXVECTOR3 up(0, 1, 0);
    D3DXMatrixLookAtLH(&View, &cameraPosition, &g_cameraTarget, &up);
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

    for (int z = 0; z < 3; ++z)
    {
        for (int y = 0; y < 3; ++y)
        {
            for (int x = 0; x < 3; ++x)
            {
                D3DXMATRIX world;
                D3DXMatrixTranslation(&world,
                                      (x - 1) * 30.0f,
                                      (y - 1) * 30.0f,
                                      z * 30.0f);

                D3DXMATRIX worldViewProj = world * View * Proj;
                hResult = g_pEffect->SetMatrix("g_matWorldViewProj", &worldViewProj);
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
            }
        }
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

