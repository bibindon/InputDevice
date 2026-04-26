#include "InputDevice.h"
#include <string>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

namespace InputDevice
{

IKeyBoard* SKeyBoard::m_keyboard = nullptr;
namespace
{
    LPDIRECTINPUT8 g_directInput = nullptr;
    HWND g_inputHWnd = nullptr;
    bool g_keyboardOwnedByLibrary = false;
    LPDIRECTINPUTDEVICE8 g_mouse = nullptr;
    DIMOUSESTATE2 g_mouseState = { };
    DIMOUSESTATE2 g_mousePrevState = { };
    std::deque<std::vector<BYTE>> g_mouseButtonDeque;
    LPDIRECTINPUTDEVICE8 g_gamePad = nullptr;
    DIJOYSTATE2 g_gamePadState = { };
    DIJOYSTATE2 g_gamePadPrevState = { };
    std::deque<std::vector<BYTE>> g_gamePadButtonDeque;
    constexpr std::size_t kMouseButtonCount = 8;
    constexpr std::size_t kGamePadButtonCount = 128;
    constexpr std::size_t kHoldFrameCount = 30;
    constexpr std::size_t kInputHistoryFrameCount = 60 * 5;

    bool IsValidMouseButtonIndex(char key)
    {
        return 0 <= key && key < static_cast<char>(kMouseButtonCount);
    }

    bool IsValidGamePadButtonIndex(char key)
    {
        return 0 <= key && static_cast<unsigned char>(key) < kGamePadButtonCount;
    }

    BOOL CALLBACK EnumGamePadCallback(const DIDEVICEINSTANCE* instance, VOID* context)
    {
        UNREFERENCED_PARAMETER(context);

        if (g_directInput == nullptr)
        {
            return DIENUM_STOP;
        }

        HRESULT ret = g_directInput->CreateDevice(instance->guidInstance, &g_gamePad, NULL);
        if (FAILED(ret))
        {
            g_gamePad = nullptr;
            return DIENUM_CONTINUE;
        }

        return DIENUM_STOP;
    }
}

void KeyBoard::Initialize(LPDIRECTINPUT8 directInput, HWND hWnd)
{
    ZeroMemory(m_key, sizeof(m_key));
    ZeroMemory(m_keyPrev, sizeof(m_keyPrev));

    HRESULT ret = directInput->CreateDevice(GUID_SysKeyboard, &m_keyboard, NULL);

    ret = m_keyboard->SetDataFormat(&c_dfDIKeyboard);

    // 排他制御のセット
    ret = m_keyboard->SetCooperativeLevel(hWnd,
                                          DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);

    // 動作開始
    ret = m_keyboard->Acquire();
}

void SKeyBoard::Set(IKeyBoard* arg)
{
    if (m_keyboard != nullptr && m_keyboard != arg && g_keyboardOwnedByLibrary)
    {
        m_keyboard->Finalize();
        delete m_keyboard;
        m_keyboard = nullptr;
        g_keyboardOwnedByLibrary = false;
    }

    m_keyboard = arg;
}

IKeyBoard* SKeyBoard::Get()
{
    return m_keyboard;
}

void SKeyBoard::Update()
{
    if (m_keyboard == nullptr)
    {
        return;
    }

    m_keyboard->Update();
}

bool SKeyBoard::IsDown(int keyCode)
{
    if (m_keyboard == nullptr)
    {
        return false;
    }

    return m_keyboard->IsDown(keyCode);
}

bool SKeyBoard::IsDownFirstFrame(int keyCode)
{
    if (m_keyboard == nullptr)
    {
        return false;
    }

    return m_keyboard->IsDownFirstFrame(keyCode);
}

bool SKeyBoard::IsHold(int keyCode)
{
    if (m_keyboard == nullptr)
    {
        return false;
    }

    return m_keyboard->IsHold(keyCode);
}

void KeyBoard::Update()
{
    // キーの入力
    memcpy(m_keyPrev, m_key, 256);
    ZeroMemory(m_key, sizeof(m_key));
    HRESULT ret = m_keyboard->GetDeviceState(sizeof(m_key), m_key);
    if (FAILED(ret))
    {
        // 失敗なら再開させてもう一度取得
        m_keyboard->Acquire();
        m_keyboard->GetDeviceState(sizeof(m_key), m_key);
    }

    std::vector<BYTE> temp(256);
    std::copy(&m_key[0], &m_key[256], temp.begin());
    m_keyDeque.push_front(temp);

    // 5秒分のキー情報、以上のキー情報が保存されているなら消す
    if (m_keyDeque.size() >= 60 * 5)
    {
        m_keyDeque.erase(m_keyDeque.begin() + 60 * 5, m_keyDeque.end());
    }

    if (false)
    {
        for (int i = 0; i < 256; ++i)
        {
            auto str = std::to_wstring(m_keyDeque.at(0).at(i)) + L" ";
            OutputDebugString(str.c_str());
        }

        OutputDebugString(L"\n");
    }
}

void KeyBoard::Finalize()
{
    std::deque<std::vector<BYTE>> emptyDeque;
    m_keyDeque.swap(emptyDeque);

    if (m_keyboard != nullptr)
    {
        m_keyboard->Unacquire();
        m_keyboard->Release();
        m_keyboard = nullptr;
    }
}

bool KeyBoard::IsDown(int keyCode)
{
    if (m_key[keyCode] & 0x80)
    {
        return true;
    }
    return false;
}

bool KeyBoard::IsDownFirstFrame(int keyCode)
{
    if (m_key[keyCode] & 0x80)
    {
        if ((m_keyPrev[keyCode] & 0x80) == 0)
        {
            return true;
        }
    }
    return false;
}

bool KeyBoard::IsHold(int keyCode)
{
    // 500ミリ秒以上押されていたら長押しと判断する
    if (m_keyDeque.size() <= 30)
    {
        return false;
    }

    bool isHold = true;
    for (std::size_t i = 0; i < 30; ++i)
    {
        if (m_keyDeque.at(i).at((std::size_t)keyCode) & 0x80)
        {
            continue;
        }
        else
        {
            isHold = false;
            break;
        }
    }

    if (isHold)
    {
        return true;
    }

    return isHold;
}

void MockKeyBoard::Initialize(LPDIRECTINPUT8 directInput, HWND hWnd)
{
    UNREFERENCED_PARAMETER(directInput);
    UNREFERENCED_PARAMETER(hWnd);

    ZeroMemory(m_key, sizeof(m_key));
    ZeroMemory(m_keyPrev, sizeof(m_keyPrev));
    m_keyDeque.clear();
}

void MockKeyBoard::Update()
{
    memcpy(m_keyPrev, m_key, sizeof(m_key));

    std::vector<BYTE> temp(256);
    std::copy(&m_key[0], &m_key[256], temp.begin());
    m_keyDeque.push_front(temp);

    if (m_keyDeque.size() >= 60 * 5)
    {
        m_keyDeque.erase(m_keyDeque.begin() + 60 * 5, m_keyDeque.end());
    }
}

void MockKeyBoard::Finalize()
{
    ClearAllKeys();
    ZeroMemory(m_keyPrev, sizeof(m_keyPrev));

    std::deque<std::vector<BYTE>> emptyDeque;
    m_keyDeque.swap(emptyDeque);
}

bool MockKeyBoard::IsDown(int keyCode)
{
    if (m_key[keyCode] & 0x80)
    {
        return true;
    }
    return false;
}

bool MockKeyBoard::IsDownFirstFrame(int keyCode)
{
    if (m_key[keyCode] & 0x80)
    {
        if ((m_keyPrev[keyCode] & 0x80) == 0)
        {
            return true;
        }
    }
    return false;
}

bool MockKeyBoard::IsHold(int keyCode)
{
    if (m_keyDeque.size() <= 30)
    {
        return false;
    }

    bool isHold = true;
    for (std::size_t i = 0; i < 30; ++i)
    {
        if (m_keyDeque.at(i).at((std::size_t)keyCode) & 0x80)
        {
            continue;
        }
        else
        {
            isHold = false;
            break;
        }
    }

    if (isHold)
    {
        return true;
    }

    return isHold;
}

void MockKeyBoard::SetKeyDown(int keyCode, bool isDown)
{
    if (isDown)
    {
        m_key[keyCode] = 0x80;
    }
    else
    {
        m_key[keyCode] = 0x00;
    }
}

void MockKeyBoard::ClearAllKeys()
{
    ZeroMemory(m_key, sizeof(m_key));
}

bool Mouse::Initialize()
{
    if (g_directInput == nullptr || g_inputHWnd == nullptr)
    {
        return false;
    }

    if (g_mouse != nullptr)
    {
        return true;
    }

    ZeroMemory(&g_mouseState, sizeof(g_mouseState));
    ZeroMemory(&g_mousePrevState, sizeof(g_mousePrevState));
    g_mouseButtonDeque.clear();

    HRESULT ret = g_directInput->CreateDevice(GUID_SysMouse, &g_mouse, NULL);
    if (FAILED(ret))
    {
        g_mouse = nullptr;
        return false;
    }

    ret = g_mouse->SetDataFormat(&c_dfDIMouse2);
    if (FAILED(ret))
    {
        g_mouse->Release();
        g_mouse = nullptr;
        return false;
    }

    ret = g_mouse->SetCooperativeLevel(g_inputHWnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
    if (FAILED(ret))
    {
        g_mouse->Release();
        g_mouse = nullptr;
        return false;
    }

    ret = g_mouse->Acquire();
    if (FAILED(ret))
    {
        ZeroMemory(&g_mouseState, sizeof(g_mouseState));
        ZeroMemory(&g_mousePrevState, sizeof(g_mousePrevState));
    }

    return true;
}

bool Mouse::Finalize()
{
    ZeroMemory(&g_mouseState, sizeof(g_mouseState));
    ZeroMemory(&g_mousePrevState, sizeof(g_mousePrevState));

    std::deque<std::vector<BYTE>> emptyDeque;
    g_mouseButtonDeque.swap(emptyDeque);

    if (g_mouse != nullptr)
    {
        g_mouse->Unacquire();
        g_mouse->Release();
        g_mouse = nullptr;
    }

    return true;
}

bool Mouse::Update()
{
    if (g_mouse == nullptr)
    {
        return false;
    }

    memcpy(&g_mousePrevState, &g_mouseState, sizeof(g_mouseState));
    ZeroMemory(&g_mouseState, sizeof(g_mouseState));

    HRESULT ret = g_mouse->GetDeviceState(sizeof(g_mouseState), &g_mouseState);
    if (FAILED(ret))
    {
        ret = g_mouse->Acquire();
        if (FAILED(ret))
        {
            return false;
        }

        ret = g_mouse->GetDeviceState(sizeof(g_mouseState), &g_mouseState);
        if (FAILED(ret))
        {
            return false;
        }
    }

    std::vector<BYTE> temp(kMouseButtonCount);
    std::copy(&g_mouseState.rgbButtons[0],
              &g_mouseState.rgbButtons[kMouseButtonCount],
              temp.begin());
    g_mouseButtonDeque.push_front(temp);

    if (g_mouseButtonDeque.size() >= kInputHistoryFrameCount)
    {
        g_mouseButtonDeque.erase(g_mouseButtonDeque.begin() + kInputHistoryFrameCount,
                                 g_mouseButtonDeque.end());
    }

    return true;
}

bool Mouse::IsDown(const char key)
{
    if (!IsValidMouseButtonIndex(key))
    {
        return false;
    }

    return (g_mouseState.rgbButtons[(std::size_t)key] & 0x80) != 0;
}

bool Mouse::IsDownFirstFrame(const char key)
{
    if (!IsValidMouseButtonIndex(key))
    {
        return false;
    }

    const std::size_t index = (std::size_t)key;
    const bool isDown = (g_mouseState.rgbButtons[index] & 0x80) != 0;
    const bool wasDown = (g_mousePrevState.rgbButtons[index] & 0x80) != 0;
    return isDown && !wasDown;
}

bool Mouse::IsHold(const char key)
{
    if (!IsValidMouseButtonIndex(key))
    {
        return false;
    }

    if (g_mouseButtonDeque.size() <= kHoldFrameCount)
    {
        return false;
    }

    for (std::size_t i = 0; i < kHoldFrameCount; ++i)
    {
        if ((g_mouseButtonDeque.at(i).at((std::size_t)key) & 0x80) == 0)
        {
            return false;
        }
    }

    return true;
}

bool Mouse::IsUp(const char key)
{
    if (!IsValidMouseButtonIndex(key))
    {
        return false;
    }

    return (g_mouseState.rgbButtons[(std::size_t)key] & 0x80) == 0;
}

bool GamePad::Initialize()
{
    if (g_directInput == nullptr || g_inputHWnd == nullptr)
    {
        return false;
    }

    if (g_gamePad != nullptr)
    {
        return true;
    }

    ZeroMemory(&g_gamePadState, sizeof(g_gamePadState));
    ZeroMemory(&g_gamePadPrevState, sizeof(g_gamePadPrevState));
    g_gamePadButtonDeque.clear();

    HRESULT ret = g_directInput->EnumDevices(DI8DEVCLASS_GAMECTRL,
                                             EnumGamePadCallback,
                                             NULL,
                                             DIEDFL_ATTACHEDONLY);
    if (FAILED(ret) || g_gamePad == nullptr)
    {
        return false;
    }

    ret = g_gamePad->SetDataFormat(&c_dfDIJoystick2);
    if (FAILED(ret))
    {
        g_gamePad->Release();
        g_gamePad = nullptr;
        return false;
    }

    ret = g_gamePad->SetCooperativeLevel(g_inputHWnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
    if (FAILED(ret))
    {
        g_gamePad->Release();
        g_gamePad = nullptr;
        return false;
    }

    ret = g_gamePad->Acquire();
    if (FAILED(ret))
    {
        ZeroMemory(&g_gamePadState, sizeof(g_gamePadState));
        ZeroMemory(&g_gamePadPrevState, sizeof(g_gamePadPrevState));
    }

    return true;
}

bool GamePad::Finalize()
{
    ZeroMemory(&g_gamePadState, sizeof(g_gamePadState));
    ZeroMemory(&g_gamePadPrevState, sizeof(g_gamePadPrevState));

    std::deque<std::vector<BYTE>> emptyDeque;
    g_gamePadButtonDeque.swap(emptyDeque);

    if (g_gamePad != nullptr)
    {
        g_gamePad->Unacquire();
        g_gamePad->Release();
        g_gamePad = nullptr;
    }

    return true;
}

bool GamePad::Update()
{
    if (g_gamePad == nullptr)
    {
        return false;
    }

    memcpy(&g_gamePadPrevState, &g_gamePadState, sizeof(g_gamePadState));
    ZeroMemory(&g_gamePadState, sizeof(g_gamePadState));

    HRESULT ret = g_gamePad->Poll();
    if (FAILED(ret))
    {
        ret = g_gamePad->Acquire();
        if (FAILED(ret))
        {
            return false;
        }
    }

    ret = g_gamePad->GetDeviceState(sizeof(g_gamePadState), &g_gamePadState);
    if (FAILED(ret))
    {
        ret = g_gamePad->Acquire();
        if (FAILED(ret))
        {
            return false;
        }

        ret = g_gamePad->GetDeviceState(sizeof(g_gamePadState), &g_gamePadState);
        if (FAILED(ret))
        {
            return false;
        }
    }

    std::vector<BYTE> temp(kGamePadButtonCount);
    std::copy(&g_gamePadState.rgbButtons[0],
              &g_gamePadState.rgbButtons[kGamePadButtonCount],
              temp.begin());
    g_gamePadButtonDeque.push_front(temp);

    if (g_gamePadButtonDeque.size() >= kInputHistoryFrameCount)
    {
        g_gamePadButtonDeque.erase(g_gamePadButtonDeque.begin() + kInputHistoryFrameCount,
                                   g_gamePadButtonDeque.end());
    }

    return true;
}

bool GamePad::IsDown(const char key)
{
    if (!IsValidGamePadButtonIndex(key))
    {
        return false;
    }

    return (g_gamePadState.rgbButtons[(std::size_t)key] & 0x80) != 0;
}

bool GamePad::IsDownFirstFrame(const char key)
{
    if (!IsValidGamePadButtonIndex(key))
    {
        return false;
    }

    const std::size_t index = (std::size_t)key;
    const bool isDown = (g_gamePadState.rgbButtons[index] & 0x80) != 0;
    const bool wasDown = (g_gamePadPrevState.rgbButtons[index] & 0x80) != 0;
    return isDown && !wasDown;
}

bool GamePad::IsHold(const char key)
{
    if (!IsValidGamePadButtonIndex(key))
    {
        return false;
    }

    if (g_gamePadButtonDeque.size() <= kHoldFrameCount)
    {
        return false;
    }

    for (std::size_t i = 0; i < kHoldFrameCount; ++i)
    {
        if ((g_gamePadButtonDeque.at(i).at((std::size_t)key) & 0x80) == 0)
        {
            return false;
        }
    }

    return true;
}

bool GamePad::IsUp(const char key)
{
    if (!IsValidGamePadButtonIndex(key))
    {
        return false;
    }

    return (g_gamePadState.rgbButtons[(std::size_t)key] & 0x80) == 0;
}

// モックキーボードクラスを使いたい場合は、
// この関数を呼ばずに、
// IKeyBoardクラスを継承した独自のクラスを作って
// SKeyBoard::Set関数に渡せばよい
void Initialize(HINSTANCE hInstance, HWND hWnd)
{
    IKeyBoard* keyboard = new KeyBoard();
    HRESULT hr = DirectInput8Create(hInstance,
                                    DIRECTINPUT_VERSION,
                                    IID_IDirectInput8,
                                    (void**)&g_directInput,
                                    NULL);

    g_inputHWnd = hWnd;
    keyboard->Initialize(g_directInput, hWnd);
    SKeyBoard::Set(keyboard);
    g_keyboardOwnedByLibrary = true;
    Mouse::Initialize();
    GamePad::Initialize();
}

void Update()
{
    SKeyBoard::Update();
    Mouse::Update();
    GamePad::Update();
}

void Finalize()
{
    GamePad::Finalize();
    Mouse::Finalize();

    IKeyBoard* keyboard = SKeyBoard::Get();
    if (keyboard != nullptr && g_keyboardOwnedByLibrary)
    {
        keyboard->Finalize();
        delete keyboard;
        g_keyboardOwnedByLibrary = false;
        SKeyBoard::Set(nullptr);
    }

    if (g_directInput != nullptr)
    {
        g_directInput->Release();
        g_directInput = nullptr;
    }

    g_inputHWnd = nullptr;
}

}

