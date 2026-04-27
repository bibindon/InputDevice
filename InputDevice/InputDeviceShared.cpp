#include "InputDeviceInternal.h"
#include <cmath>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "Xinput.lib")

namespace InputDevice
{

IKeyBoard* SKeyBoard::m_keyboard = nullptr;

namespace Internal
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
bool g_mouseCursorVisible = true;
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

const std::size_t kMouseButtonCount = 8;
const std::size_t kGamePadButtonCount = 128;
const std::size_t kGamePadXButtonStateCount = 132;
const std::size_t kHoldFrameCount = 30;
const std::size_t kInputHistoryFrameCount = 60 * 5;
const LONG kGamePadAxisMin = -1000;
const LONG kGamePadAxisMax = 1000;
const float kGamePadStickDeadZone = 0.05f;
const BYTE kGamePadXTriggerThreshold = 30;
const ULONGLONG kGamePadSearchIntervalMilliseconds = 5000;

bool IsValidMouseButtonIndex(char key)
{
    return 0 <= key && key < static_cast<char>(kMouseButtonCount);
}

bool GetMouseWindowCenterScreenPosition(POINT* centerPosition)
{
    if (centerPosition == nullptr)
    {
        return false;
    }

    if (g_inputHWnd == nullptr)
    {
        return false;
    }

    RECT clientRect = { };
    if (!GetClientRect(g_inputHWnd, &clientRect))
    {
        return false;
    }

    POINT clientCenter = { };
    clientCenter.x = (clientRect.right - clientRect.left) / 2;
    clientCenter.y = (clientRect.bottom - clientRect.top) / 2;

    if (!ClientToScreen(g_inputHWnd, &clientCenter))
    {
        return false;
    }

    centerPosition->x = clientCenter.x;
    centerPosition->y = clientCenter.y;
    return true;
}

void ApplyMouseCursorVisible(bool isVisible)
{
    if (isVisible)
    {
        while (ShowCursor(TRUE) < 0)
        {
        }
    }
    else
    {
        while (ShowCursor(FALSE) >= 0)
        {
        }
    }

    g_mouseCursorVisible = isVisible;
}

void CenterMouseCursorInWindow()
{
    POINT centerPosition = { };
    if (!GetMouseWindowCenterScreenPosition(&centerPosition))
    {
        return;
    }

    SetCursorPos(centerPosition.x, centerPosition.y);
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

bool IsMouseCursorInWindow()
{
    if (g_inputHWnd == nullptr)
    {
        return false;
    }

    POINT cursorPosition = { };
    if (!GetCursorPos(&cursorPosition))
    {
        return false;
    }

    if (!ScreenToClient(g_inputHWnd, &cursorPosition))
    {
        return false;
    }

    RECT clientRect = { };
    if (!GetClientRect(g_inputHWnd, &clientRect))
    {
        return false;
    }

    return PtInRect(&clientRect, cursorPosition) != FALSE;
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

int GetUnifiedInputKeyCode(GamePadButton button)
{
    if (button == GAMEPAD_POV_UP)
    {
        return DIK_UP;
    }

    if (button == GAMEPAD_POV_RIGHT)
    {
        return DIK_RIGHT;
    }

    if (button == GAMEPAD_POV_DOWN)
    {
        return DIK_DOWN;
    }

    if (button == GAMEPAD_POV_LEFT)
    {
        return DIK_LEFT;
    }

    return -1;
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

GamePadStick CreateStickFromFloatAxis(float x, float y)
{
    GamePadStick stick = { };

    stick.x = ClampFloat(x, -1.0f, 1.0f);
    stick.y = ClampFloat(y, -1.0f, 1.0f);
    stick.x = ApplyGamePadStickDeadZone(stick.x);
    stick.y = ApplyGamePadStickDeadZone(stick.y);

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
}
