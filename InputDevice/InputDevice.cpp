#include "InputDeviceInternal.h"

namespace InputDevice
{

using namespace Internal;

void Initialize(HINSTANCE hInstance, HWND hWnd)
{
    IKeyBoard* keyboard = new KeyBoard();

    // DirectInput8Create は DirectInput 全体の入口を作る関数。
    // ここで得た g_directInput からキーボード、マウス、ゲームパッド用の
    // 個別デバイスオブジェクトを作っていく。
    HRESULT hr = DirectInput8Create(hInstance,
                                    DIRECTINPUT_VERSION,
                                    IID_IDirectInput8,
                                    (void**)&g_directInput,
                                    NULL);
    UNREFERENCED_PARAMETER(hr);

    g_inputHWnd = hWnd;

    // 各入力デバイスは「どのウィンドウに対する入力か」を必要とする。
    // そのため最初に対象ウィンドウを保持してから初期化する。
    keyboard->Initialize(g_directInput, hWnd);
    SKeyBoard::Set(keyboard);
    g_keyboardOwnedByLibrary = true;
    Mouse::Initialize();
    g_gamePadD.Initialize();
    g_gamePadX.Initialize();

    // 抜き差し対応では再接続タイマーを使うため、
    // 起動直後の時刻をここでそろえておく。
    g_lastMouseReconnectTime = GetTickCount64();
    g_lastKeyboardReconnectTime = GetTickCount64();
}

void Update()
{
    // 毎フレームここを呼ぶことで、
    // それぞれのクラスが「現在状態」「前フレーム状態」「履歴」を更新する。
    SKeyBoard::Update();
    Mouse::Update();
    g_gamePadD.Update();
    g_gamePadX.Update();
}

void Finalize()
{
    // 逆順で破棄していくと依存関係を追いやすい。
    // 先に個別デバイスを解放し、最後に DirectInput 本体を解放する。
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

}
