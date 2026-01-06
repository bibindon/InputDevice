#include "InputDevice.h"

namespace InputDevice
{

bool KeyBoard::Initialize()
{
    return false;
}

bool KeyBoard::Finalize()
{
    return false;
}

bool KeyBoard::Update()
{
    return false;
}

bool KeyBoard::IsDown(const char key)
{
    return false;
}

bool KeyBoard::IsHold(const char key)
{
    return false;
}

bool KeyBoard::IsUp(const char key)
{
    return false;
}

bool Mouse::Initialize()
{
    return false;
}

bool Mouse::Finalize()
{
    return false;
}

bool Mouse::Update()
{
    return false;
}

bool Mouse::IsDown(const char key)
{
    return false;
}

bool Mouse::IsHold(const char key)
{
    return false;
}

bool Mouse::IsUp(const char key)
{
    return false;
}

bool GamePad::Initialize()
{
    return false;
}

bool GamePad::Finalize()
{
    return false;
}

bool GamePad::Update()
{
    return false;
}

bool GamePad::IsDown(const char key)
{
    return false;
}

bool GamePad::IsHold(const char key)
{
    return false;
}

bool GamePad::IsUp(const char key)
{
    return false;
}

}

