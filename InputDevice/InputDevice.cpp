#include "InputDevice.h"
#include <cmath>
#include <string>
#include <Xinput.h>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "Xinput.lib")

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
    MousePosition g_mousePosition = { };
    MousePosition g_mousePrevPosition = { };
    MousePosition g_mouseDelta = { };
    std::deque<std::vector<BYTE>> g_mouseButtonDeque;
    LPDIRECTINPUTDEVICE8 g_gamePad = nullptr;
    DIJOYSTATE2 g_gamePadState = { };
    DIJOYSTATE2 g_gamePadPrevState = { };
    std::deque<std::vector<BYTE>> g_gamePadButtonDeque;
    std::deque<DWORD> g_gamePadPOVDeque;
    ULONGLONG g_lastGamePadSearchTime = 0;
    XINPUT_STATE g_gamePadXState = { };
    XINPUT_STATE g_gamePadXPrevState = { };
    std::deque<std::vector<BYTE>> g_gamePadXButtonDeque;
    bool g_gamePadXConnected = false;
    GamePad_D g_gamePadD;
    GamePad_X g_gamePadX;
    constexpr std::size_t kMouseButtonCount = 8;
    constexpr std::size_t kGamePadButtonCount = 128;
    constexpr std::size_t kGamePadXButtonStateCount = 132;
    constexpr std::size_t kHoldFrameCount = 30;
    constexpr std::size_t kInputHistoryFrameCount = 60 * 5;
    constexpr LONG kGamePadAxisMin = -1000;
    constexpr LONG kGamePadAxisMax = 1000;
    constexpr float kGamePadStickDeadZone = 0.05f;
    constexpr BYTE kGamePadXTriggerThreshold = 30;
    constexpr ULONGLONG kGamePadSearchIntervalMilliseconds = 5000;

    bool IsValidMouseButtonIndex(char key)
    {
        return 0 <= key && key < static_cast<char>(kMouseButtonCount);
    }

    void UpdateMousePosition()
    {
        if (g_inputHWnd == nullptr)
        {
            g_mousePosition.x = 0;
            g_mousePosition.y = 0;
            return;
        }

        POINT cursorPosition = { };
        if (!GetCursorPos(&cursorPosition))
        {
            g_mousePosition.x = 0;
            g_mousePosition.y = 0;
            return;
        }

        if (!ScreenToClient(g_inputHWnd, &cursorPosition))
        {
            g_mousePosition.x = 0;
            g_mousePosition.y = 0;
            return;
        }

        g_mousePosition.x = cursorPosition.x;
        g_mousePosition.y = cursorPosition.y;
    }

    bool IsValidGamePadButtonIndex(GamePadButton button)
    {
        int buttonIndex = static_cast<int>(button);
        return 0 <= buttonIndex && static_cast<std::size_t>(buttonIndex) < kGamePadButtonCount;
    }

    bool IsValidGamePadXButtonStateIndex(GamePadButton button)
    {
        int buttonIndex = static_cast<int>(button);
        return 0 <= buttonIndex && static_cast<std::size_t>(buttonIndex) < kGamePadXButtonStateCount;
    }

    bool IsGamePadPOVButton(GamePadButton button)
    {
        if (button == GAMEPAD_POV_UP)
        {
            return true;
        }

        if (button == GAMEPAD_POV_RIGHT)
        {
            return true;
        }

        if (button == GAMEPAD_POV_DOWN)
        {
            return true;
        }

        if (button == GAMEPAD_POV_LEFT)
        {
            return true;
        }

        return false;
    }

    float ClampFloat(float value, float minValue, float maxValue)
    {
        if (value < minValue)
        {
            return minValue;
        }

        if (maxValue < value)
        {
            return maxValue;
        }

        return value;
    }

    float NormalizeGamePadAxis(LONG axis)
    {
        float value = static_cast<float>(axis) / static_cast<float>(kGamePadAxisMax);
        return ClampFloat(value, -1.0f, 1.0f);
    }

    float ApplyGamePadStickDeadZone(float value)
    {
        if (-kGamePadStickDeadZone <= value && value <= kGamePadStickDeadZone)
        {
            return 0.0f;
        }

        return value;
    }

    GamePadStick CreateGamePadStick(LONG xAxis, LONG yAxis)
    {
        GamePadStick stick = { };
        stick.x = ApplyGamePadStickDeadZone(NormalizeGamePadAxis(xAxis));
        stick.y = ApplyGamePadStickDeadZone(-NormalizeGamePadAxis(yAxis));

        stick.power = std::sqrt((stick.x * stick.x) + (stick.y * stick.y));
        stick.power = ClampFloat(stick.power, 0.0f, 1.0f);

        if (stick.power <= 0.0f)
        {
            stick.angle = 0.0f;
            return stick;
        }

        stick.angle = std::atan2(stick.y, stick.x);
        stick.angle = ApplyGamePadStickDeadZone(stick.angle);
        return stick;
    }

    LONG ConvertXInputAxisToGamePadAxis(SHORT axis)
    {
        float value = static_cast<float>(axis) / 32767.0f;
        value = ClampFloat(value, -1.0f, 1.0f);
        return static_cast<LONG>(value * static_cast<float>(kGamePadAxisMax));
    }

    bool IsGamePadXButtonPressed(GamePadButton button, const XINPUT_STATE& state)
    {
        WORD buttons = state.Gamepad.wButtons;

        if (button == GAMEPAD_A)
        {
            return (buttons & XINPUT_GAMEPAD_A) != 0;
        }

        if (button == GAMEPAD_B)
        {
            return (buttons & XINPUT_GAMEPAD_B) != 0;
        }

        if (button == GAMEPAD_X)
        {
            return (buttons & XINPUT_GAMEPAD_X) != 0;
        }

        if (button == GAMEPAD_Y)
        {
            return (buttons & XINPUT_GAMEPAD_Y) != 0;
        }

        if (button == GAMEPAD_L1)
        {
            return (buttons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0;
        }

        if (button == GAMEPAD_R1)
        {
            return (buttons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0;
        }

        if (button == GAMEPAD_L2)
        {
            return kGamePadXTriggerThreshold <= state.Gamepad.bLeftTrigger;
        }

        if (button == GAMEPAD_R2)
        {
            return kGamePadXTriggerThreshold <= state.Gamepad.bRightTrigger;
        }

        if (button == GAMEPAD_BACK)
        {
            return (buttons & XINPUT_GAMEPAD_BACK) != 0;
        }

        if (button == GAMEPAD_START)
        {
            return (buttons & XINPUT_GAMEPAD_START) != 0;
        }

        if (button == GAMEPAD_POV_UP)
        {
            return (buttons & XINPUT_GAMEPAD_DPAD_UP) != 0;
        }

        if (button == GAMEPAD_POV_RIGHT)
        {
            return (buttons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0;
        }

        if (button == GAMEPAD_POV_DOWN)
        {
            return (buttons & XINPUT_GAMEPAD_DPAD_DOWN) != 0;
        }

        if (button == GAMEPAD_POV_LEFT)
        {
            return (buttons & XINPUT_GAMEPAD_DPAD_LEFT) != 0;
        }

        return false;
    }

    IGamePad* GetActiveGamePad()
    {
        if (g_gamePadXConnected)
        {
            return &g_gamePadX;
        }

        if (g_gamePad != nullptr)
        {
            return &g_gamePadD;
        }

        return nullptr;
    }

    void SetGamePadXButtonState(std::vector<BYTE>* buttonState, GamePadButton button)
    {
        if (buttonState == nullptr)
        {
            return;
        }

        if (!IsValidGamePadXButtonStateIndex(button))
        {
            return;
        }

        std::size_t index = static_cast<std::size_t>(button);
        if (IsGamePadXButtonPressed(button, g_gamePadXState))
        {
            buttonState->at(index) = 0x80;
        }
        else
        {
            buttonState->at(index) = 0x00;
        }
    }

    void SetGamePadAxisRange(DWORD objectOffset)
    {
        if (g_gamePad == nullptr)
        {
            return;
        }

        DIPROPRANGE range;
        ZeroMemory(&range, sizeof(range));
        range.diph.dwSize = sizeof(range);
        range.diph.dwHeaderSize = sizeof(range.diph);
        range.diph.dwObj = objectOffset;
        range.diph.dwHow = DIPH_BYOFFSET;
        range.lMin = kGamePadAxisMin;
        range.lMax = kGamePadAxisMax;

        g_gamePad->SetProperty(DIPROP_RANGE, &range.diph);
    }

    void ReleaseGamePadDDevice()
    {
        ZeroMemory(&g_gamePadState, sizeof(g_gamePadState));
        ZeroMemory(&g_gamePadPrevState, sizeof(g_gamePadPrevState));

        std::deque<std::vector<BYTE>> emptyButtonDeque;
        g_gamePadButtonDeque.swap(emptyButtonDeque);

        std::deque<DWORD> emptyPOVDeque;
        g_gamePadPOVDeque.swap(emptyPOVDeque);

        if (g_gamePad != nullptr)
        {
            g_gamePad->Unacquire();
            g_gamePad->Release();
            g_gamePad = nullptr;
        }
    }

    bool IsGamePadPOVPressed(DWORD pov, DWORD minValue, DWORD maxValue)
    {
        if (pov == 0xFFFFFFFF)
        {
            return false;
        }

        if (minValue <= maxValue)
        {
            if (minValue <= pov && pov <= maxValue)
            {
                return true;
            }

            return false;
        }

        if (minValue <= pov || pov <= maxValue)
        {
            return true;
        }

        return false;
    }

    bool IsGamePadCurrentPOVPressed(DWORD minValue, DWORD maxValue)
    {
        return IsGamePadPOVPressed(g_gamePadState.rgdwPOV[0], minValue, maxValue);
    }

    bool IsGamePadPrevPOVPressed(DWORD minValue, DWORD maxValue)
    {
        return IsGamePadPOVPressed(g_gamePadPrevState.rgdwPOV[0], minValue, maxValue);
    }

    bool IsGamePadPOVFirstFrame(DWORD minValue, DWORD maxValue)
    {
        bool isDown = IsGamePadCurrentPOVPressed(minValue, maxValue);
        bool wasDown = IsGamePadPrevPOVPressed(minValue, maxValue);
        return isDown && !wasDown;
    }

    bool IsGamePadPOVHold(DWORD minValue, DWORD maxValue)
    {
        if (g_gamePadPOVDeque.size() <= kHoldFrameCount)
        {
            return false;
        }

        for (std::size_t i = 0; i < kHoldFrameCount; ++i)
        {
            if (!IsGamePadPOVPressed(g_gamePadPOVDeque.at(i), minValue, maxValue))
            {
                return false;
            }
        }

        return true;
    }

    bool IsGamePadCurrentPOVButtonPressed(GamePadButton button)
    {
        if (button == GAMEPAD_POV_UP)
        {
            return IsGamePadCurrentPOVPressed(31500, 4500);
        }

        if (button == GAMEPAD_POV_RIGHT)
        {
            return IsGamePadCurrentPOVPressed(4500, 13500);
        }

        if (button == GAMEPAD_POV_DOWN)
        {
            return IsGamePadCurrentPOVPressed(13500, 22500);
        }

        if (button == GAMEPAD_POV_LEFT)
        {
            return IsGamePadCurrentPOVPressed(22500, 31500);
        }

        return false;
    }

    bool IsGamePadPOVButtonFirstFrame(GamePadButton button)
    {
        if (button == GAMEPAD_POV_UP)
        {
            return IsGamePadPOVFirstFrame(31500, 4500);
        }

        if (button == GAMEPAD_POV_RIGHT)
        {
            return IsGamePadPOVFirstFrame(4500, 13500);
        }

        if (button == GAMEPAD_POV_DOWN)
        {
            return IsGamePadPOVFirstFrame(13500, 22500);
        }

        if (button == GAMEPAD_POV_LEFT)
        {
            return IsGamePadPOVFirstFrame(22500, 31500);
        }

        return false;
    }

    bool IsGamePadPOVButtonHold(GamePadButton button)
    {
        if (button == GAMEPAD_POV_UP)
        {
            return IsGamePadPOVHold(31500, 4500);
        }

        if (button == GAMEPAD_POV_RIGHT)
        {
            return IsGamePadPOVHold(4500, 13500);
        }

        if (button == GAMEPAD_POV_DOWN)
        {
            return IsGamePadPOVHold(13500, 22500);
        }

        if (button == GAMEPAD_POV_LEFT)
        {
            return IsGamePadPOVHold(22500, 31500);
        }

        return false;
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
    g_mousePosition = { };
    g_mousePrevPosition = { };
    g_mouseDelta = { };
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
    g_mousePosition = { };
    g_mousePrevPosition = { };
    g_mouseDelta = { };

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
    // 前回の位置を保存
    g_mousePrevPosition = g_mousePosition;

    UpdateMousePosition();
    g_mouseDelta.x = 0;
    g_mouseDelta.y = 0;

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

    // Delta はカーソル座標差分ではなく DirectInput の移動量を使う。
    g_mouseDelta.x = g_mouseState.lX;
    g_mouseDelta.y = g_mouseState.lY;

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

MousePosition Mouse::GetPosition()
{
    return g_mousePosition;
}

MousePosition Mouse::GetDelta(GamePadStick* stick)
{
    if (stick != nullptr)
    {
        float dx = static_cast<float>(g_mouseDelta.x);

        // マウス座標は下方向が正、スティックは上方向が正
        float dy = -static_cast<float>(g_mouseDelta.y);
        
        float magnitude = std::sqrt(dx * dx + dy * dy);
        
        if (magnitude > 0.0f)
        {
            float power = 0.f;

            // 5ピクセルをpower 1.0の基準として扱う
            power = magnitude / 5.0f;
            
            stick->x = (dx / magnitude) * power;
            stick->y = (dy / magnitude) * power;
            stick->power = power;
            stick->angle = std::atan2(stick->y, stick->x);
        }
        else
        {
            stick->x = 0.0f;
            stick->y = 0.0f;
            stick->power = 0.0f;
            stick->angle = 0.0f;
        }
    }
    
    return g_mouseDelta;
}

bool GamePad_D::Initialize()
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
    g_gamePadPOVDeque.clear();
    g_lastGamePadSearchTime = GetTickCount64();

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

    SetGamePadAxisRange(DIJOFS_X);
    SetGamePadAxisRange(DIJOFS_Y);
    SetGamePadAxisRange(DIJOFS_Z);
    SetGamePadAxisRange(DIJOFS_RZ);

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

bool GamePad_D::Finalize()
{
    g_lastGamePadSearchTime = 0;
    ReleaseGamePadDDevice();

    return true;
}

bool GamePad_D::Update()
{
    if (g_gamePad == nullptr)
    {
        ULONGLONG currentTime = GetTickCount64();
        if (g_lastGamePadSearchTime == 0 ||
            currentTime - g_lastGamePadSearchTime >= kGamePadSearchIntervalMilliseconds)
        {
            g_gamePadD.Initialize();
        }

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
            ReleaseGamePadDDevice();
            return false;
        }
    }

    ret = g_gamePad->GetDeviceState(sizeof(g_gamePadState), &g_gamePadState);
    if (FAILED(ret))
    {
        ret = g_gamePad->Acquire();
        if (FAILED(ret))
        {
            ReleaseGamePadDDevice();
            return false;
        }

        ret = g_gamePad->GetDeviceState(sizeof(g_gamePadState), &g_gamePadState);
        if (FAILED(ret))
        {
            ReleaseGamePadDDevice();
            return false;
        }
    }

    std::vector<BYTE> temp(kGamePadButtonCount);
    std::copy(&g_gamePadState.rgbButtons[0],
              &g_gamePadState.rgbButtons[kGamePadButtonCount],
              temp.begin());
    g_gamePadButtonDeque.push_front(temp);
    g_gamePadPOVDeque.push_front(g_gamePadState.rgdwPOV[0]);

    if (g_gamePadButtonDeque.size() >= kInputHistoryFrameCount)
    {
        g_gamePadButtonDeque.erase(g_gamePadButtonDeque.begin() + kInputHistoryFrameCount,
                                   g_gamePadButtonDeque.end());
    }

    if (g_gamePadPOVDeque.size() >= kInputHistoryFrameCount)
    {
        g_gamePadPOVDeque.erase(g_gamePadPOVDeque.begin() + kInputHistoryFrameCount,
                                g_gamePadPOVDeque.end());
    }

    return true;
}

bool GamePad_D::IsDown(GamePadButton button)
{
    if (IsGamePadPOVButton(button))
    {
        return IsGamePadCurrentPOVButtonPressed(button);
    }

    if (!IsValidGamePadButtonIndex(button))
    {
        return false;
    }

    return (g_gamePadState.rgbButtons[(std::size_t)button] & 0x80) != 0;
}

bool GamePad_D::IsDownFirstFrame(GamePadButton button)
{
    if (IsGamePadPOVButton(button))
    {
        return IsGamePadPOVButtonFirstFrame(button);
    }

    if (!IsValidGamePadButtonIndex(button))
    {
        return false;
    }

    const std::size_t index = (std::size_t)button;
    const bool isDown = (g_gamePadState.rgbButtons[index] & 0x80) != 0;
    const bool wasDown = (g_gamePadPrevState.rgbButtons[index] & 0x80) != 0;
    return isDown && !wasDown;
}

bool GamePad_D::IsHold(GamePadButton button)
{
    if (IsGamePadPOVButton(button))
    {
        return IsGamePadPOVButtonHold(button);
    }

    if (!IsValidGamePadButtonIndex(button))
    {
        return false;
    }

    if (g_gamePadButtonDeque.size() <= kHoldFrameCount)
    {
        return false;
    }

    for (std::size_t i = 0; i < kHoldFrameCount; ++i)
    {
        if ((g_gamePadButtonDeque.at(i).at((std::size_t)button) & 0x80) == 0)
        {
            return false;
        }
    }

    return true;
}

bool GamePad_D::IsUp(GamePadButton button)
{
    if (IsGamePadPOVButton(button))
    {
        return !IsGamePadCurrentPOVButtonPressed(button);
    }

    if (!IsValidGamePadButtonIndex(button))
    {
        return false;
    }

    return (g_gamePadState.rgbButtons[(std::size_t)button] & 0x80) == 0;
}

GamePadStick GamePad_D::GetStickL()
{
    if (g_gamePad == nullptr)
    {
        GamePadStick stick = { };
        return stick;
    }

    return CreateGamePadStick(g_gamePadState.lX, g_gamePadState.lY);
}

GamePadStick GamePad_D::GetStickR()
{
    if (g_gamePad == nullptr)
    {
        GamePadStick stick = { };
        return stick;
    }

    return CreateGamePadStick(g_gamePadState.lZ, g_gamePadState.lRz);
}

bool GamePad_X::Initialize()
{
    ZeroMemory(&g_gamePadXState, sizeof(g_gamePadXState));
    ZeroMemory(&g_gamePadXPrevState, sizeof(g_gamePadXPrevState));
    g_gamePadXButtonDeque.clear();

    DWORD ret = XInputGetState(0, &g_gamePadXState);
    if (ret == ERROR_SUCCESS)
    {
        g_gamePadXConnected = true;
        return true;
    }

    g_gamePadXConnected = false;
    ZeroMemory(&g_gamePadXState, sizeof(g_gamePadXState));
    return false;
}

bool GamePad_X::Finalize()
{
    ZeroMemory(&g_gamePadXState, sizeof(g_gamePadXState));
    ZeroMemory(&g_gamePadXPrevState, sizeof(g_gamePadXPrevState));
    g_gamePadXConnected = false;

    std::deque<std::vector<BYTE>> emptyDeque;
    g_gamePadXButtonDeque.swap(emptyDeque);

    return true;
}

bool GamePad_X::Update()
{
    memcpy(&g_gamePadXPrevState, &g_gamePadXState, sizeof(g_gamePadXState));
    ZeroMemory(&g_gamePadXState, sizeof(g_gamePadXState));

    DWORD ret = XInputGetState(0, &g_gamePadXState);
    if (ret != ERROR_SUCCESS)
    {
        g_gamePadXConnected = false;
        ZeroMemory(&g_gamePadXState, sizeof(g_gamePadXState));
        return false;
    }

    g_gamePadXConnected = true;

    std::vector<BYTE> temp(kGamePadXButtonStateCount);
    SetGamePadXButtonState(&temp, GAMEPAD_X);
    SetGamePadXButtonState(&temp, GAMEPAD_A);
    SetGamePadXButtonState(&temp, GAMEPAD_B);
    SetGamePadXButtonState(&temp, GAMEPAD_Y);
    SetGamePadXButtonState(&temp, GAMEPAD_L1);
    SetGamePadXButtonState(&temp, GAMEPAD_R1);
    SetGamePadXButtonState(&temp, GAMEPAD_L2);
    SetGamePadXButtonState(&temp, GAMEPAD_R2);
    SetGamePadXButtonState(&temp, GAMEPAD_BACK);
    SetGamePadXButtonState(&temp, GAMEPAD_START);
    SetGamePadXButtonState(&temp, GAMEPAD_POV_UP);
    SetGamePadXButtonState(&temp, GAMEPAD_POV_RIGHT);
    SetGamePadXButtonState(&temp, GAMEPAD_POV_DOWN);
    SetGamePadXButtonState(&temp, GAMEPAD_POV_LEFT);
    g_gamePadXButtonDeque.push_front(temp);

    if (g_gamePadXButtonDeque.size() >= kInputHistoryFrameCount)
    {
        g_gamePadXButtonDeque.erase(g_gamePadXButtonDeque.begin() + kInputHistoryFrameCount,
                                    g_gamePadXButtonDeque.end());
    }

    return true;
}

bool GamePad_X::IsDown(GamePadButton button)
{
    if (!g_gamePadXConnected)
    {
        return false;
    }

    return IsGamePadXButtonPressed(button, g_gamePadXState);
}

bool GamePad_X::IsDownFirstFrame(GamePadButton button)
{
    if (!g_gamePadXConnected)
    {
        return false;
    }

    bool isDown = IsGamePadXButtonPressed(button, g_gamePadXState);
    bool wasDown = IsGamePadXButtonPressed(button, g_gamePadXPrevState);
    return isDown && !wasDown;
}

bool GamePad_X::IsHold(GamePadButton button)
{
    if (!g_gamePadXConnected)
    {
        return false;
    }

    if (!IsValidGamePadXButtonStateIndex(button))
    {
        return false;
    }

    if (g_gamePadXButtonDeque.size() <= kHoldFrameCount)
    {
        return false;
    }

    std::size_t index = static_cast<std::size_t>(button);
    for (std::size_t i = 0; i < kHoldFrameCount; ++i)
    {
        if ((g_gamePadXButtonDeque.at(i).at(index) & 0x80) == 0)
        {
            return false;
        }
    }

    return true;
}

bool GamePad_X::IsUp(GamePadButton button)
{
    if (!g_gamePadXConnected)
    {
        return true;
    }

    return !IsGamePadXButtonPressed(button, g_gamePadXState);
}

GamePadStick GamePad_X::GetStickL()
{
    if (!g_gamePadXConnected)
    {
        GamePadStick stick = { };
        return stick;
    }

    LONG x = ConvertXInputAxisToGamePadAxis(g_gamePadXState.Gamepad.sThumbLX);
    LONG y = -ConvertXInputAxisToGamePadAxis(g_gamePadXState.Gamepad.sThumbLY);
    return CreateGamePadStick(x, y);
}

GamePadStick GamePad_X::GetStickR()
{
    if (!g_gamePadXConnected)
    {
        GamePadStick stick = { };
        return stick;
    }

    LONG x = ConvertXInputAxisToGamePadAxis(g_gamePadXState.Gamepad.sThumbRX);
    LONG y = -ConvertXInputAxisToGamePadAxis(g_gamePadXState.Gamepad.sThumbRY);
    return CreateGamePadStick(x, y);
}

IGamePad* GetGamePadD()
{
    return &g_gamePadD;
}

IGamePad* GetGamePadX()
{
    return &g_gamePadX;
}

bool GamePad::Initialize()
{
    bool isDirectInputInitialized = g_gamePadD.Initialize();
    bool isXInputInitialized = g_gamePadX.Initialize();

    if (isXInputInitialized)
    {
        return true;
    }

    if (isDirectInputInitialized)
    {
        return true;
    }

    return false;
}

bool GamePad::Finalize()
{
    bool isXInputFinalized = g_gamePadX.Finalize();
    bool isDirectInputFinalized = g_gamePadD.Finalize();

    if (isXInputFinalized && isDirectInputFinalized)
    {
        return true;
    }

    return false;
}

bool GamePad::Update()
{
    bool isDirectInputUpdated = g_gamePadD.Update();
    bool isXInputUpdated = g_gamePadX.Update();

    if (isXInputUpdated)
    {
        return true;
    }

    if (isDirectInputUpdated)
    {
        return true;
    }

    return false;
}

bool GamePad::IsDown(GamePadButton button)
{
    IGamePad* gamePad = GetActiveGamePad();
    if (gamePad == nullptr)
    {
        return false;
    }

    return gamePad->IsDown(button);
}

bool GamePad::IsDownFirstFrame(GamePadButton button)
{
    IGamePad* gamePad = GetActiveGamePad();
    if (gamePad == nullptr)
    {
        return false;
    }

    return gamePad->IsDownFirstFrame(button);
}

bool GamePad::IsHold(GamePadButton button)
{
    IGamePad* gamePad = GetActiveGamePad();
    if (gamePad == nullptr)
    {
        return false;
    }

    return gamePad->IsHold(button);
}

bool GamePad::IsUp(GamePadButton button)
{
    IGamePad* gamePad = GetActiveGamePad();
    if (gamePad == nullptr)
    {
        return true;
    }

    return gamePad->IsUp(button);
}

GamePadStick GamePad::GetStickL()
{
    IGamePad* gamePad = GetActiveGamePad();
    if (gamePad == nullptr)
    {
        GamePadStick stick = { };
        return stick;
    }

    return gamePad->GetStickL();
}

GamePadStick GamePad::GetStickR()
{
    IGamePad* gamePad = GetActiveGamePad();
    if (gamePad == nullptr)
    {
        GamePadStick stick = { };
        return stick;
    }

    return gamePad->GetStickR();
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
    g_gamePadD.Initialize();
    g_gamePadX.Initialize();
}

void Update()
{
    SKeyBoard::Update();
    Mouse::Update();
    g_gamePadD.Update();
    g_gamePadX.Update();
}

void Finalize()
{
    g_gamePadX.Finalize();
    g_gamePadD.Finalize();
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

bool UnifiedInput::Initialize()
{
    return false;
}

bool UnifiedInput::Finalize()
{
    return false;
}

bool UnifiedInput::Update()
{
    return false;
}

bool UnifiedInput::IsDown(GamePadButton button)
{
    return false;
}

bool UnifiedInput::IsDownFirstFrame(GamePadButton button)
{
    return false;
}

bool UnifiedInput::IsHold(GamePadButton button)
{
    return false;
}

bool UnifiedInput::IsUp(GamePadButton button)
{
    return false;
}

}
