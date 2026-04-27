#include "InputDeviceInternal.h"
#include <algorithm>

namespace InputDevice
{

using namespace Internal;

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

    // DirectInput のゲームパッドは OS 上のデバイス列挙から探す。
    // まずは接続済みコントローラーを1台見つけてつかむ構成。
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

    // ジョイスティック系は軸レンジが機種依存になりやすい。
    // ここで共通レンジへそろえておくと以後の正規化が簡単になる。
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
            // 未接続中は毎フレーム列挙せず、一定間隔でだけ再探索する。
            g_gamePadD.Initialize();
        }

        return false;
    }

    memcpy(&g_gamePadPrevState, &g_gamePadState, sizeof(g_gamePadState));
    ZeroMemory(&g_gamePadState, sizeof(g_gamePadState));

    HRESULT ret = g_gamePad->Poll();
    if (FAILED(ret))
    {
        // Poll はジョイスティック系で現在状態を更新するための呼び出し。
        // 失敗したら Acquire で取り直しを試す。
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
    // DirectInput の通常ボタンと POV 履歴を分けて残すことで、
    // 十字キーも First/Hold/UpFirst で扱えるようにしている。
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
        // DirectInput の十字キーは rgbButtons ではなく POV 角度で来るため、
        // 通常ボタンとは別の判定関数を通す。
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

bool GamePad_D::IsUpFirstFrame(GamePadButton button)
{
    if (IsGamePadPOVButton(button))
    {
        if (button == GAMEPAD_POV_UP)
        {
            return !IsGamePadCurrentPOVButtonPressed(button) &&
                   IsGamePadPrevPOVPressed(31500, 4500);
        }

        if (button == GAMEPAD_POV_RIGHT)
        {
            return !IsGamePadCurrentPOVButtonPressed(button) &&
                   IsGamePadPrevPOVPressed(4500, 13500);
        }

        if (button == GAMEPAD_POV_DOWN)
        {
            return !IsGamePadCurrentPOVButtonPressed(button) &&
                   IsGamePadPrevPOVPressed(13500, 22500);
        }

        if (button == GAMEPAD_POV_LEFT)
        {
            return !IsGamePadCurrentPOVButtonPressed(button) &&
                   IsGamePadPrevPOVPressed(22500, 31500);
        }

        return false;
    }

    if (!IsValidGamePadButtonIndex(button))
    {
        return false;
    }

    const std::size_t index = (std::size_t)button;
    const bool isDown = (g_gamePadState.rgbButtons[index] & 0x80) != 0;
    const bool wasDown = (g_gamePadPrevState.rgbButtons[index] & 0x80) != 0;
    return !isDown && wasDown;
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

    // XInput は DirectInput のような列挙ではなく、
    // まず 0 番パッドに問い合わせて接続有無を見る。
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

    // XInput は Poll 不要で、XInputGetState を呼ぶたびに最新状態が返る。
    DWORD ret = XInputGetState(0, &g_gamePadXState);
    if (ret != ERROR_SUCCESS)
    {
        g_gamePadXConnected = false;
        ZeroMemory(&g_gamePadXState, sizeof(g_gamePadXState));
        return false;
    }

    g_gamePadXConnected = true;

    std::vector<BYTE> temp(kGamePadXButtonStateCount);
    // XInput の各ビット状態を、ライブラリ共通の GamePadButton 添字へ詰め直す。
    // こうしておくと Hold/First 判定を DirectInput 版と同じ作りで共有できる。
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

bool GamePad_X::IsUpFirstFrame(GamePadButton button)
{
    if (!g_gamePadXConnected)
    {
        return false;
    }

    bool isDown = IsGamePadXButtonPressed(button, g_gamePadXState);
    bool wasDown = IsGamePadXButtonPressed(button, g_gamePadXPrevState);
    return !isDown && wasDown;
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

    // 戻り値も XInput 優先にしておくと、
    // 呼び出し側は「今どちらが生きているか」を細かく見なくて済む。
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

bool GamePad::IsUpFirstFrame(GamePadButton button)
{
    IGamePad* gamePad = GetActiveGamePad();
    if (gamePad == nullptr)
    {
        return false;
    }

    return gamePad->IsUpFirstFrame(button);
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

}
