#include "InputDeviceInternal.h"
#include <algorithm>
#include <cmath>

namespace InputDevice
{

using namespace Internal;

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
    ApplyMouseCursorVisible(true);

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

    g_mouseDelta.x = g_mouseState.lX;
    g_mouseDelta.y = g_mouseState.lY;

    if (!g_mouseCursorVisible)
    {
        CenterMouseCursorInWindow();
        UpdateMousePosition();
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
        float dx = static_cast<float>(g_mouseDelta.x);
        float dy = -static_cast<float>(g_mouseDelta.y);
        float magnitude = std::sqrt(dx * dx + dy * dy);

        if (magnitude > 0.0f)
        {
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
