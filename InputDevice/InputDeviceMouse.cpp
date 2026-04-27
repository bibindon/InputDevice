#include "InputDeviceInternal.h"
#include <algorithm>
#include <cmath>

namespace InputDevice
{

using namespace Internal;

namespace
{
    // マウスはボタン状態に加えて座標や相対移動量も持っている。
    // 再接続時に古い値が残ると表示も判定も壊れるため、まとめてクリアする。
    void ResetMouseState()
    {
        ZeroMemory(&g_mouseState, sizeof(g_mouseState));
        ZeroMemory(&g_mousePrevState, sizeof(g_mousePrevState));
        g_mousePosition = { };
        g_mousePrevPosition = { };
        g_mouseDelta = { };

        std::deque<std::vector<BYTE>> emptyDeque;
        g_mouseButtonDeque.swap(emptyDeque);
    }

    void ReleaseMouseDevice()
    {
        ResetMouseState();

        if (g_mouse != nullptr)
        {
            g_mouse->Unacquire();
            g_mouse->Release();
            g_mouse = nullptr;
        }
    }

    bool TryReconnectMouseDevice()
    {
        if (g_directInput == nullptr || g_inputHWnd == nullptr)
        {
            return false;
        }

        // GUID_SysMouse は OS が管理している標準マウス入力口。
        // 物理的に抜き差ししても、再取得すると再び入力を読めることが多い。
        HRESULT ret = g_directInput->CreateDevice(GUID_SysMouse, &g_mouse, NULL);
        if (FAILED(ret))
        {
            g_mouse = nullptr;
            return false;
        }

        ret = g_mouse->SetDataFormat(&c_dfDIMouse2);
        if (FAILED(ret))
        {
            ReleaseMouseDevice();
            return false;
        }

        ret = g_mouse->SetCooperativeLevel(g_inputHWnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
        if (FAILED(ret))
        {
            ReleaseMouseDevice();
            return false;
        }

        ret = g_mouse->Acquire();
        if (FAILED(ret) && ret != DIERR_OTHERAPPHASPRIO && ret != DIERR_NOTACQUIRED)
        {
            ReleaseMouseDevice();
            return false;
        }

        ResetMouseState();
        return true;
    }
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
    g_mouseCursorVisible = true;
    g_mouseButtonDeque.clear();

    // マウスはホイールやサイドボタンまで取りたいので、
    // c_dfDIMouse2 を使って DIMOUSESTATE2 形式で読む。
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

    // FOREGROUND + NONEXCLUSIVE にすることで、
    // アプリが前面にある間だけ、OS と共有しながら入力を受け取る。
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
    ApplyMouseCursorVisible(true);
    ReleaseMouseDevice();

    return true;
}

bool Mouse::Update()
{
    ULONGLONG currentTime = GetTickCount64();

    if (g_mouse == nullptr)
    {
        // デバイスをいったん失った場合だけ、
        // 一定間隔で作り直しを試す。
        if (currentTime - g_lastMouseReconnectTime >= kGamePadSearchIntervalMilliseconds &&
            g_directInput != nullptr)
        {
            g_lastMouseReconnectTime = currentTime;
            TryReconnectMouseDevice();
        }
        return false;
    }

    g_mousePrevPosition = g_mousePosition;

    UpdateMousePosition();
    g_mouseDelta.x = 0;
    g_mouseDelta.y = 0;

    memcpy(&g_mousePrevState, &g_mouseState, sizeof(g_mouseState));
    ZeroMemory(&g_mouseState, sizeof(g_mouseState));

    // DIMOUSESTATE2 には
    // rgbButtons: ボタン
    // lX/lY: 相対移動量
    // lZ: ホイール量
    // が入る。
    HRESULT ret = g_mouse->GetDeviceState(sizeof(g_mouseState), &g_mouseState);
    if (FAILED(ret))
    {
        bool isRecovered = false;

        if (ret == DIERR_INPUTLOST)
        {
            // Alt+Tab などで入力を失っただけなら Acquire で戻せる。
            ret = g_mouse->Acquire();
            if (SUCCEEDED(ret))
            {
                ret = g_mouse->GetDeviceState(sizeof(g_mouseState), &g_mouseState);
                if (SUCCEEDED(ret))
                {
                    isRecovered = true;
                }
            }

            if (!isRecovered &&
                (ret == DIERR_OTHERAPPHASPRIO || ret == DIERR_NOTACQUIRED || ret == DIERR_INPUTLOST))
            {
                return false;
            }
        }
        else if (ret == DIERR_NOTACQUIRED || ret == DIERR_OTHERAPPHASPRIO)
        {
            return false;
        }

        if (!isRecovered)
        {
            // 一時的なフォーカスロストではなく復旧不能なら、
            // デバイスを破棄して後続フレームで再作成する。
            ReleaseMouseDevice();
            g_lastMouseReconnectTime = currentTime;
            return false;
        }
    }

    // GetDelta はカーソル座標差ではなく、
    // DirectInput が返す生の相対移動量をそのまま使う。
    // これにより、カーソルを中央へ戻しても入力量は失われない。
    g_mouseDelta.x = g_mouseState.lX;
    g_mouseDelta.y = g_mouseState.lY;

    if (!g_mouseCursorVisible)
    {
        // FPS視点のような操作をしやすくするため、
        // 非表示中は毎フレーム中央へ戻す。
        CenterMouseCursorInWindow();
        UpdateMousePosition();
    }

    // ボタンの履歴は Hold 判定用。
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

bool Mouse::IsDown(MouseButton key)
{
    if (!IsValidMouseButtonIndex(key))
    {
        return false;
    }

    return (g_mouseState.rgbButtons[(std::size_t)key] & 0x80) != 0;
}

bool Mouse::IsDownFirstFrame(MouseButton key)
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

bool Mouse::IsHold(MouseButton key)
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

bool Mouse::IsUpFirstFrame(MouseButton key)
{
    if (!IsValidMouseButtonIndex(key))
    {
        return false;
    }

    const std::size_t index = (std::size_t)key;
    const bool isDown = (g_mouseState.rgbButtons[index] & 0x80) != 0;
    const bool wasDown = (g_mousePrevState.rgbButtons[index] & 0x80) != 0;
    return !isDown && wasDown;
}

bool Mouse::IsInWindow()
{
    return IsMouseCursorInWindow();
}

bool Mouse::IsVisible()
{
    return g_mouseCursorVisible;
}

void Mouse::SetVisible(bool isVisible)
{
    ApplyMouseCursorVisible(isVisible);

    if (!isVisible)
    {
        CenterMouseCursorInWindow();
        UpdateMousePosition();
        g_mousePrevPosition = g_mousePosition;
    }
}

MousePosition Mouse::GetPosition()
{
    return g_mousePosition;
}

long Mouse::GetWheelDelta()
{
    return g_mouseState.lZ;
}

MousePosition Mouse::GetDelta(GamePadStick* stick)
{
    if (stick != nullptr)
    {
        // マウス移動を「右スティックのような値」としても使えるようにする。
        // dx, dy から方向と強さを作り、UnifiedInput 側でゲームパッドと合成する。
        float dx = static_cast<float>(g_mouseDelta.x);
        float dy = -static_cast<float>(g_mouseDelta.y);
        float magnitude = std::sqrt(dx * dx + dy * dy);

        if (magnitude > 0.0f)
        {
            // 5 ピクセル移動で power 1.0 相当。
            // それ以上速く動かしたときは 1.0 を超えてもよい設計にしている。
            float power = magnitude / 5.0f;

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

}
