#include "InputDeviceInternal.h"
#include <algorithm>
#include <string>

namespace InputDevice
{

using namespace Internal;

void KeyBoard::Initialize(LPDIRECTINPUT8 directInput, HWND hWnd)
{
    ZeroMemory(m_key, sizeof(m_key));
    ZeroMemory(m_keyPrev, sizeof(m_keyPrev));

    HRESULT ret = directInput->CreateDevice(GUID_SysKeyboard, &m_keyboard, NULL);

    ret = m_keyboard->SetDataFormat(&c_dfDIKeyboard);

    ret = m_keyboard->SetCooperativeLevel(hWnd,
                                          DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);

    ret = m_keyboard->Acquire();
    UNREFERENCED_PARAMETER(ret);
}

void SKeyBoard::Set(IKeyBoard* arg)
{
    if (m_keyboard != nullptr && m_keyboard != arg && g_keyboardOwnedByLibrary)
    {
        m_keyboard->Finalize();
        delete m_keyboard;
        m_keyboard = nullptr;
        g_keyboardOwnedByLibrary = false;
    }

    m_keyboard = arg;
}

IKeyBoard* SKeyBoard::Get()
{
    return m_keyboard;
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

bool SKeyBoard::IsUpFirstFrame(int keyCode)
{
    if (m_keyboard == nullptr)
    {
        return false;
    }

    return m_keyboard->IsUpFirstFrame(keyCode);
}

void KeyBoard::Update()
{
    memcpy(m_keyPrev, m_key, 256);
    ZeroMemory(m_key, sizeof(m_key));
    HRESULT ret = m_keyboard->GetDeviceState(sizeof(m_key), m_key);
    if (FAILED(ret))
    {
        m_keyboard->Acquire();
        m_keyboard->GetDeviceState(sizeof(m_key), m_key);
    }

    std::vector<BYTE> temp(256);
    std::copy(&m_key[0], &m_key[256], temp.begin());
    m_keyDeque.push_front(temp);

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
    std::deque<std::vector<BYTE>> emptyDeque;
    m_keyDeque.swap(emptyDeque);

    if (m_keyboard != nullptr)
    {
        m_keyboard->Unacquire();
        m_keyboard->Release();
        m_keyboard = nullptr;
    }
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

        isHold = false;
        break;
    }

    if (isHold)
    {
        return true;
    }

    return isHold;
}

bool KeyBoard::IsUpFirstFrame(int keyCode)
{
    if ((m_key[keyCode] & 0x80) == 0)
    {
        if (m_keyPrev[keyCode] & 0x80)
        {
            return true;
        }
    }

    return false;
}

void MockKeyBoard::Initialize(LPDIRECTINPUT8 directInput, HWND hWnd)
{
    UNREFERENCED_PARAMETER(directInput);
    UNREFERENCED_PARAMETER(hWnd);

    ZeroMemory(m_key, sizeof(m_key));
    ZeroMemory(m_keyPrev, sizeof(m_keyPrev));
    m_keyDeque.clear();
}

void MockKeyBoard::Update()
{
    memcpy(m_keyPrev, m_key, sizeof(m_key));

    std::vector<BYTE> temp(256);
    std::copy(&m_key[0], &m_key[256], temp.begin());
    m_keyDeque.push_front(temp);

    if (m_keyDeque.size() >= 60 * 5)
    {
        m_keyDeque.erase(m_keyDeque.begin() + 60 * 5, m_keyDeque.end());
    }
}

void MockKeyBoard::Finalize()
{
    ClearAllKeys();
    ZeroMemory(m_keyPrev, sizeof(m_keyPrev));

    std::deque<std::vector<BYTE>> emptyDeque;
    m_keyDeque.swap(emptyDeque);
}

bool MockKeyBoard::IsDown(int keyCode)
{
    if (m_key[keyCode] & 0x80)
    {
        return true;
    }
    return false;
}

bool MockKeyBoard::IsDownFirstFrame(int keyCode)
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

bool MockKeyBoard::IsHold(int keyCode)
{
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

        isHold = false;
        break;
    }

    if (isHold)
    {
        return true;
    }

    return isHold;
}

bool MockKeyBoard::IsUpFirstFrame(int keyCode)
{
    if ((m_key[keyCode] & 0x80) == 0)
    {
        if (m_keyPrev[keyCode] & 0x80)
        {
            return true;
        }
    }

    return false;
}

void MockKeyBoard::SetKeyDown(int keyCode, bool isDown)
{
    if (isDown)
    {
        m_key[keyCode] = 0x80;
    }
    else
    {
        m_key[keyCode] = 0x00;
    }
}

void MockKeyBoard::ClearAllKeys()
{
    ZeroMemory(m_key, sizeof(m_key));
}

}
