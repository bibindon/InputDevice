#include "InputDeviceInternal.h"

namespace InputDevice
{

using namespace Internal;

void Initialize(HINSTANCE hInstance, HWND hWnd)
{
    IKeyBoard* keyboard = new KeyBoard();
    HRESULT hr = DirectInput8Create(hInstance,
                                    DIRECTINPUT_VERSION,
                                    IID_IDirectInput8,
                                    (void**)&g_directInput,
                                    NULL);
    UNREFERENCED_PARAMETER(hr);

    g_inputHWnd = hWnd;
    keyboard->Initialize(g_directInput, hWnd);
    SKeyBoard::Set(keyboard);
    g_keyboardOwnedByLibrary = true;
    Mouse::Initialize();
    g_gamePadD.Initialize();
    g_gamePadX.Initialize();
}

void Update()
{
    SKeyBoard::Update();
    Mouse::Update();
    g_gamePadD.Update();
    g_gamePadX.Update();
}

void Finalize()
{
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
