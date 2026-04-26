
#pragma once

#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif

#include <dinput.h>
#include <deque>
#include <vector>

namespace InputDevice
{

void Initialize(HINSTANCE hInstance, HWND hWnd);
void Update();
void Finalize();

// テストコードでモックキーボードクラスを作って使いたい。
// そのために継承を使う。
// 継承を使うためstaticにできない。
class IKeyBoard
{
public:
    virtual ~IKeyBoard() = default;
    virtual void Initialize(LPDIRECTINPUT8 directInput, HWND hWnd) = 0;
    virtual void Update() = 0;
    virtual void Finalize() = 0;

    // 押されている
    virtual bool IsDown(int keyCode) = 0;

    // 押されていて、その直前まで押されていなかった
    virtual bool IsDownFirstFrame(int keyCode) = 0;

    // 0.5秒以上長押ししていた
    virtual bool IsHold(int keyCode) = 0;

};

class KeyBoard : public IKeyBoard
{
public:
    void Initialize(LPDIRECTINPUT8 directInput, HWND hWnd);
    void Update();
    void Finalize();

    // 押されている
    bool IsDown(int keyCode);

    // 押されていて、その直前まで押されていなかった
    bool IsDownFirstFrame(int keyCode);

    // 0.5秒以上長押ししていた
    bool IsHold(int keyCode);

private:
    LPDIRECTINPUTDEVICE8 m_keyboard;
    BYTE m_key[256];
    BYTE m_keyPrev[256];
    std::deque<std::vector<BYTE>> m_keyDeque;
};

class MockKeyBoard : public IKeyBoard
{
public:
    void Initialize(LPDIRECTINPUT8 directInput, HWND hWnd);
    void Update();
    void Finalize();

    bool IsDown(int keyCode);
    bool IsDownFirstFrame(int keyCode);
    bool IsHold(int keyCode);

    void SetKeyDown(int keyCode, bool isDown);
    void ClearAllKeys();

private:
    BYTE m_key[256];
    BYTE m_keyPrev[256];
    std::deque<std::vector<BYTE>> m_keyDeque;
};

// 楽に使用するためのstaticクラス
class SKeyBoard
{
public:
    static void Set(IKeyBoard* arg);
    static IKeyBoard* Get();

    static void Update();

    // 押されている
    static bool IsDown(int keyCode);

    // 押されていて、その直前まで押されていなかった
    static bool IsDownFirstFrame(int keyCode);

    // 0.5秒以上長押ししていた
    static bool IsHold(int keyCode);
private:
    static IKeyBoard* m_keyboard;
};

class Mouse
{
public:
    static bool Initialize();
    static bool Finalize();
    static bool Update();

    static bool IsDown(const char key);
    static bool IsDownFirstFrame(const char key);
    static bool IsHold(const char key);
    static bool IsUp(const char key);

private:
};

// ゲームパッドにはDirectInputとXInputがある。
// こちらはDirectInput
// 末尾のDが目印
enum GamePadButton
{
    GAMEPAD_X = 0,
    GAMEPAD_A = 1,
    GAMEPAD_B = 2,
    GAMEPAD_Y = 3,
    GAMEPAD_L1 = 4,
    GAMEPAD_R1 = 5,
    GAMEPAD_L2 = 6,
    GAMEPAD_R2 = 7,
    GAMEPAD_BACK = 8,
    GAMEPAD_START = 9,
    GAMEPAD_POV_UP = 128,
    GAMEPAD_POV_RIGHT = 129,
    GAMEPAD_POV_DOWN = 130,
    GAMEPAD_POV_LEFT = 131,
};

struct GamePadStick
{
    // x/yは-1.0～1.0。yは上方向をプラスとして扱う。
    float x;
    float y;
    // powerは倒れ具合。0.0～1.0。
    float power;
    // angleは方向角。右が0、上が約1.57ラジアン。
    float angle;
};

class GamePad_D
{
public:
    static bool Initialize();
    static bool Finalize();
    static bool Update();

    static bool IsDown(GamePadButton button);
    static bool IsDownFirstFrame(GamePadButton button);
    static bool IsHold(GamePadButton button);
    static bool IsUp(GamePadButton button);
    static GamePadStick GetStickL();
    static GamePadStick GetStickR();

private:
};

// こちらはXInput
// 末尾のXが目印
class GamePad_X
{
public:
    static bool Initialize();
    static bool Finalize();
    static bool Update();

    static bool IsDown(GamePadButton button);
    static bool IsDownFirstFrame(GamePadButton button);
    static bool IsHold(GamePadButton button);
    static bool IsUp(GamePadButton button);
    static GamePadStick GetStickL();
    static GamePadStick GetStickR();

private:
};

// キーボード、マウス、ゲームパッドを意識しなくてよい入力用クラス。
// 例えば、IsDown(GAMEPAD_POV_UP)を実行すると
// ゲームパッドの十字キーの上を押されているときにtrueが返ってくるが
// キーボードのWを押していてもtrueが返ってくる。
class UnifiedInput
{
public:
    static bool Initialize();
    static bool Finalize();
    static bool Update();

    static bool IsDown(GamePadButton button);
    static bool IsDownFirstFrame(GamePadButton button);
    static bool IsHold(GamePadButton button);
    static bool IsUp(GamePadButton button);

    //
    static GamePadStick GetStickL();
    static GamePadStick GetStickR();

private:
};


}

