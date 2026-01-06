#pragma once

namespace InputDevice
{

class KeyBoard
{
public:
    static bool Initialize();
    static bool Finalize();
    static bool Update();

    static bool IsDown(const char key);
    static bool IsHold(const char key);
    static bool IsUp(const char key);

private:
};

class Mouse
{
public:
    static bool Initialize();
    static bool Finalize();
    static bool Update();

    static bool IsDown(const char key);
    static bool IsHold(const char key);
    static bool IsUp(const char key);

private:
};

class GamePad
{
public:
    static bool Initialize();
    static bool Finalize();
    static bool Update();

    static bool IsDown(const char key);
    static bool IsHold(const char key);
    static bool IsUp(const char key);

private:
};

}

