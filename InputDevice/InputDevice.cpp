#include "InputDevice.h"
#include <string>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

namespace InputDevice
{

IKeyBoard* SKeyBoard::m_keyboard = nullptr;

void KeyBoard::Initialize(LPDIRECTINPUT8 directInput, HWND hWnd)
{
    HRESULT ret = directInput->CreateDevice(GUID_SysKeyboard, &m_keyboard, NULL);

    ret = m_keyboard->SetDataFormat(&c_dfDIKeyboard);

    // 排他制御のセット
    ret = m_keyboard->SetCooperativeLevel(hWnd,
                                          DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);

    // 動作開始
    ret = m_keyboard->Acquire();
}

void SKeyBoard::Set(IKeyBoard* arg)
{
    m_keyboard = arg;
}

void SKeyBoard::Update()
{
    if (m_keyboard == nullptr)
    {
        return;
    }

    m_keyboard->Update();
}

bool SKeyBoard::IsDown(int keyCode)
{
    if (m_keyboard == nullptr)
    {
        return false;
    }

    return m_keyboard->IsDown(keyCode);
}

bool SKeyBoard::IsDownFirstFrame(int keyCode)
{
    if (m_keyboard == nullptr)
    {
        return false;
    }

    return m_keyboard->IsDownFirstFrame(keyCode);
}

bool SKeyBoard::IsHold(int keyCode)
{
    if (m_keyboard == nullptr)
    {
        return false;
    }

    return m_keyboard->IsHold(keyCode);
}

void KeyBoard::Update()
{
    // キーの入力
    memcpy(m_keyPrev, m_key, 256);
    ZeroMemory(m_key, sizeof(m_key));
    HRESULT ret = m_keyboard->GetDeviceState(sizeof(m_key), m_key);
    if (FAILED(ret))
    {
        // 失敗なら再開させてもう一度取得
        m_keyboard->Acquire();
        m_keyboard->GetDeviceState(sizeof(m_key), m_key);
    }

    std::vector<BYTE> temp(256);
    std::copy(&m_key[0], &m_key[256], temp.begin());
    m_keyDeque.push_front(temp);

    // 5秒分のキー情報、以上のキー情報が保存されているなら消す
    if (m_keyDeque.size() >= 60 * 5)
    {
        m_keyDeque.erase(m_keyDeque.begin() + 60 * 5, m_keyDeque.end());
    }

    if (false)
    {
        for (int i = 0; i < 256; ++i)
        {
            auto str = std::to_wstring(m_keyDeque.at(0).at(i)) + L" ";
            OutputDebugString(str.c_str());
        }

        OutputDebugString(L"\n");
    }
}

void KeyBoard::Finalize()
{
    m_keyboard->Release();
    m_keyboard = nullptr;
}

bool KeyBoard::IsDown(int keyCode)
{
    if (m_key[keyCode] & 0x80)
    {
        return true;
    }
    return false;
}

bool KeyBoard::IsDownFirstFrame(int keyCode)
{
    if (m_key[keyCode] & 0x80)
    {
        if ((m_keyPrev[keyCode] & 0x80) == 0)
        {
            return true;
        }
    }
    return false;
}

bool KeyBoard::IsHold(int keyCode)
{
    // 500ミリ秒以上押されていたら長押しと判断する
    if (m_keyDeque.size() <= 30)
    {
        return false;
    }

    bool isHold = true;
    for (std::size_t i = 0; i < 30; ++i)
    {
        if (m_keyDeque.at(i).at((std::size_t)keyCode) & 0x80)
        {
            continue;
        }
        else
        {
            isHold = false;
            break;
        }
    }

    if (isHold)
    {
        return true;
    }

    return isHold;
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

// モックキーボードクラスを使いたい場合は、
// この関数を呼ばずに、
// IKeyBoardクラスを継承した独自のクラスを作って
// SKeyBoard::Set関数に渡せばよい
void InitializeInputDevice(HINSTANCE hInstance, HWND hWnd)
{
    LPDIRECTINPUT8 directInput = nullptr;
    IKeyBoard* keyboard = new KeyBoard();
    HRESULT hr = DirectInput8Create(hInstance, 
                                    DIRECTINPUT_VERSION,
                                    IID_IDirectInput8,
                                    (void**)&directInput,
                                    NULL);

    keyboard->Initialize(directInput, hWnd);
    SKeyBoard::Set(keyboard);
}

}

