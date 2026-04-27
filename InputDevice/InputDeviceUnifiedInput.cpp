#include "InputDeviceInternal.h"

namespace InputDevice
{

using namespace Internal;

namespace
{
    bool IsUnifiedInputKeyTriggered(GamePadButton button, bool(*predicate)(int))
    {
        if (predicate == nullptr)
        {
            return false;
        }

        auto range = g_unifiedInputKeyMap.equal_range(static_cast<int>(button));
        for (auto it = range.first; it != range.second; ++it)
        {
            if (predicate(it->second))
            {
                return true;
            }
        }

        return false;
    }
}

bool UnifiedInput::Initialize()
{
    ResetUnifiedInputKeyMap();
    return true;
}

bool UnifiedInput::Finalize()
{
    ResetUnifiedInputKeyMap();
    return true;
}

bool UnifiedInput::Update()
{
    return true;
}

bool UnifiedInput::IsDown(GamePadButton button)
{
    if (GamePad::IsDown(button))
    {
        return true;
    }

    if (IsUnifiedInputKeyTriggered(button, SKeyBoard::IsDown))
    {
        return true;
    }

    int mouseButton = GetUnifiedInputMouseButton(button);
    if (0 <= mouseButton)
    {
        return Mouse::IsDown(static_cast<MouseButton>(mouseButton));
    }

    return false;
}

bool UnifiedInput::IsDownFirstFrame(GamePadButton button)
{
    if (GamePad::IsDownFirstFrame(button))
    {
        return true;
    }

    if (IsUnifiedInputKeyTriggered(button, SKeyBoard::IsDownFirstFrame))
    {
        return true;
    }

    int mouseButton = GetUnifiedInputMouseButton(button);
    if (0 <= mouseButton)
    {
        return Mouse::IsDownFirstFrame(static_cast<MouseButton>(mouseButton));
    }

    return false;
}

bool UnifiedInput::IsHold(GamePadButton button)
{
    if (GamePad::IsHold(button))
    {
        return true;
    }

    if (IsUnifiedInputKeyTriggered(button, SKeyBoard::IsHold))
    {
        return true;
    }

    int mouseButton = GetUnifiedInputMouseButton(button);
    if (0 <= mouseButton)
    {
        return Mouse::IsHold(static_cast<MouseButton>(mouseButton));
    }

    return false;
}

bool UnifiedInput::IsUpFirstFrame(GamePadButton button)
{
    if (GamePad::IsUpFirstFrame(button))
    {
        return true;
    }

    if (IsUnifiedInputKeyTriggered(button, SKeyBoard::IsUpFirstFrame))
    {
        return true;
    }

    int mouseButton = GetUnifiedInputMouseButton(button);
    if (0 <= mouseButton)
    {
        return Mouse::IsUpFirstFrame(static_cast<MouseButton>(mouseButton));
    }

    return false;
}

void UnifiedInput::SetKeyCode(GamePadButton button, int keyCode)
{
    if (!IsValidGamePadXButtonStateIndex(button))
    {
        return;
    }

    g_unifiedInputKeyMap.emplace(static_cast<int>(button), keyCode);
}

GamePadStick UnifiedInput::GetStickL()
{
    GamePadStick gamePadStick = GamePad::GetStickL();
    float x = gamePadStick.x;
    float y = gamePadStick.y;

    if (SKeyBoard::IsDown(DIK_A))
    {
        x -= 1.0f;
    }

    if (SKeyBoard::IsDown(DIK_D))
    {
        x += 1.0f;
    }

    if (SKeyBoard::IsDown(DIK_W))
    {
        y += 1.0f;
    }

    if (SKeyBoard::IsDown(DIK_S))
    {
        y -= 1.0f;
    }

    return CreateStickFromFloatAxis(x, y);
}

GamePadStick UnifiedInput::GetStickR()
{
    GamePadStick gamePadStick = GamePad::GetStickR();
    GamePadStick mouseStick = { };
    Mouse::GetDelta(&mouseStick);

    float x = gamePadStick.x + mouseStick.x;
    float y = gamePadStick.y + mouseStick.y;

    return CreateStickFromFloatAxis(x, y);
}

}
