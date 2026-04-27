#include "InputDeviceInternal.h"

namespace InputDevice
{

using namespace Internal;

bool UnifiedInput::Initialize()
{
    return true;
}

bool UnifiedInput::Finalize()
{
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

    int keyCode = GetUnifiedInputKeyCode(button);
    if (keyCode < 0)
    {
        return false;
    }

    return SKeyBoard::IsDown(keyCode);
}

bool UnifiedInput::IsDownFirstFrame(GamePadButton button)
{
    if (GamePad::IsDownFirstFrame(button))
    {
        return true;
    }

    int keyCode = GetUnifiedInputKeyCode(button);
    if (keyCode < 0)
    {
        return false;
    }

    return SKeyBoard::IsDownFirstFrame(keyCode);
}

bool UnifiedInput::IsHold(GamePadButton button)
{
    if (GamePad::IsHold(button))
    {
        return true;
    }

    int keyCode = GetUnifiedInputKeyCode(button);
    if (keyCode < 0)
    {
        return false;
    }

    return SKeyBoard::IsHold(keyCode);
}

bool UnifiedInput::IsUpFirstFrame(GamePadButton button)
{
    if (GamePad::IsUpFirstFrame(button))
    {
        return true;
    }

    int keyCode = GetUnifiedInputKeyCode(button);
    if (keyCode < 0)
    {
        return false;
    }

    return SKeyBoard::IsUpFirstFrame(keyCode);
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
