#include "InputDeviceInternal.h"
#include <algorithm>
#include <string>

namespace InputDevice
{

using namespace Internal;

namespace
{
    // キーボード切断や再接続のタイミングでは、
    // 直前の状態が残っていると First/Hold 判定が壊れる。
    // そのため入力バッファと履歴をまとめて初期化する。
    void ResetKeyboardState(BYTE key[256],
                            BYTE keyPrev[256],
                            std::deque<std::vector<BYTE>>* keyDeque)
    {
        ZeroMemory(key, 256);
        ZeroMemory(keyPrev, 256);

        if (keyDeque != nullptr)
        {
            std::deque<std::vector<BYTE>> emptyDeque;
            keyDeque->swap(emptyDeque);
        }
    }

    void ReleaseKeyboardDevice(LPDIRECTINPUTDEVICE8* keyboard,
                               BYTE key[256],
                               BYTE keyPrev[256],
                               std::deque<std::vector<BYTE>>* keyDeque)
    {
        ResetKeyboardState(key, keyPrev, keyDeque);

        if (keyboard != nullptr && *keyboard != nullptr)
        {
            (*keyboard)->Unacquire();
            (*keyboard)->Release();
            *keyboard = nullptr;
        }
    }

    bool TryReconnectKeyboardDevice(LPDIRECTINPUTDEVICE8* keyboard,
                                    BYTE key[256],
                                    BYTE keyPrev[256],
                                    std::deque<std::vector<BYTE>>* keyDeque)
    {
        if (keyboard == nullptr)
        {
            return false;
        }

        if (g_directInput == nullptr || g_inputHWnd == nullptr)
        {
            return false;
        }

        // GUID_SysKeyboard は「現在のシステムキーボード」を表す。
        // 物理機器が変わっても、OS が用意するキーボード入力口を取り直すイメージ。
        HRESULT reconnectRet = g_directInput->CreateDevice(GUID_SysKeyboard, keyboard, NULL);
        if (FAILED(reconnectRet))
        {
            *keyboard = nullptr;
            return false;
        }

        reconnectRet = (*keyboard)->SetDataFormat(&c_dfDIKeyboard);
        if (FAILED(reconnectRet))
        {
            ReleaseKeyboardDevice(keyboard, key, keyPrev, keyDeque);
            return false;
        }

        // FOREGROUND:
        //   このアプリがアクティブな間だけ入力を受け取る。
        // NONEXCLUSIVE:
        //   他のアプリや OS と入力を共有する。
        // NOWINKEY:
        //   Windows キーによる誤操作を減らしたい意図。
        reconnectRet = (*keyboard)->SetCooperativeLevel(g_inputHWnd,
                                                        DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
        if (FAILED(reconnectRet))
        {
            ReleaseKeyboardDevice(keyboard, key, keyPrev, keyDeque);
            return false;
        }

        reconnectRet = (*keyboard)->Acquire();
        if (FAILED(reconnectRet) &&
            reconnectRet != DIERR_OTHERAPPHASPRIO &&
            reconnectRet != DIERR_NOTACQUIRED)
        {
            ReleaseKeyboardDevice(keyboard, key, keyPrev, keyDeque);
            return false;
        }

        ResetKeyboardState(key, keyPrev, keyDeque);
        return true;
    }
}

std::wstring GetKeyName(int keyCode)
{
    switch (keyCode)
    {
    case DIK_A: return L"A";
    case DIK_B: return L"B";
    case DIK_C: return L"C";
    case DIK_D: return L"D";
    case DIK_E: return L"E";
    case DIK_F: return L"F";
    case DIK_G: return L"G";
    case DIK_H: return L"H";
    case DIK_I: return L"I";
    case DIK_J: return L"J";
    case DIK_K: return L"K";
    case DIK_L: return L"L";
    case DIK_M: return L"M";
    case DIK_N: return L"N";
    case DIK_O: return L"O";
    case DIK_P: return L"P";
    case DIK_Q: return L"Q";
    case DIK_R: return L"R";
    case DIK_S: return L"S";
    case DIK_T: return L"T";
    case DIK_U: return L"U";
    case DIK_V: return L"V";
    case DIK_W: return L"W";
    case DIK_X: return L"X";
    case DIK_Y: return L"Y";
    case DIK_Z: return L"Z";
    case DIK_0: return L"0";
    case DIK_1: return L"1";
    case DIK_2: return L"2";
    case DIK_3: return L"3";
    case DIK_4: return L"4";
    case DIK_5: return L"5";
    case DIK_6: return L"6";
    case DIK_7: return L"7";
    case DIK_8: return L"8";
    case DIK_9: return L"9";
    case DIK_SPACE: return L"Space";
    case DIK_RETURN: return L"Enter";
    case DIK_ESCAPE: return L"Escape";
    case DIK_TAB: return L"Tab";
    case DIK_BACK: return L"Backspace";
    case DIK_LSHIFT: return L"LShift";
    case DIK_RSHIFT: return L"RShift";
    case DIK_LCONTROL: return L"LCtrl";
    case DIK_RCONTROL: return L"RCtrl";
    case DIK_LALT: return L"LAlt";
    case DIK_RALT: return L"RAlt";
    case DIK_UP: return L"Up";
    case DIK_DOWN: return L"Down";
    case DIK_LEFT: return L"Left";
    case DIK_RIGHT: return L"Right";
    default:
    {
        wchar_t buffer[32];
        _snwprintf_s(buffer, 32, _TRUNCATE, L"DIK_%d", keyCode);
        return buffer;
    }
    }
}

void KeyBoard::Initialize(LPDIRECTINPUT8 directInput, HWND hWnd)
{
    ZeroMemory(m_key, sizeof(m_key));
    ZeroMemory(m_keyPrev, sizeof(m_keyPrev));

    // DirectInput では
    // 1. CreateDevice
    // 2. SetDataFormat
    // 3. SetCooperativeLevel
    // 4. Acquire
    // の順で使い始めるのが基本になる。
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

bool SKeyBoard::IsHoldDuration(int keyCode, float seconds)
{
    if (m_keyboard == nullptr)
    {
        return false;
    }

    return m_keyboard->IsHoldDuration(keyCode, seconds);
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
    ULONGLONG currentTime = GetTickCount64();

    if (m_keyboard == nullptr)
    {
        // 本当にデバイスを作り直す必要がある場合だけ、
        // 一定間隔で再接続を試す。
        if (currentTime - g_lastKeyboardReconnectTime >= kGamePadSearchIntervalMilliseconds &&
            g_directInput != nullptr)
        {
            g_lastKeyboardReconnectTime = currentTime;
            TryReconnectKeyboardDevice(&m_keyboard, m_key, m_keyPrev, &m_keyDeque);
        }

        return;
    }

    memcpy(m_keyPrev, m_key, 256);
    ZeroMemory(m_key, sizeof(m_key));

    // GetDeviceState で 256 個のキー状態をまとめて取得する。
    // 各要素の 0x80 ビットが立っていれば「押されている」。
    HRESULT ret = m_keyboard->GetDeviceState(sizeof(m_key), m_key);
    if (FAILED(ret))
    {
        bool isRecovered = false;

        if (ret == DIERR_INPUTLOST ||
            ret == DIERR_NOTACQUIRED ||
            ret == DIERR_OTHERAPPHASPRIO)
        {
            // Alt+Tab などで入力を失っただけなら Acquire で復帰できる。
            // すぐ復帰できる場合はデバイスを作り直さない。
            ret = m_keyboard->Acquire();
            if (SUCCEEDED(ret))
            {
                ret = m_keyboard->GetDeviceState(sizeof(m_key), m_key);
                if (SUCCEEDED(ret))
                {
                    isRecovered = true;
                }
            }

            if (!isRecovered &&
                (ret == DIERR_OTHERAPPHASPRIO || ret == DIERR_NOTACQUIRED || ret == DIERR_INPUTLOST))
            {
                return;
            }
        }
        if (!isRecovered)
        {
            // ここまで来たら一時的なロストではなく、
            // 再作成が必要な状態とみなして解放する。
            ReleaseKeyboardDevice(&m_keyboard, m_key, m_keyPrev, &m_keyDeque);
            g_lastKeyboardReconnectTime = currentTime;
            return;
        }
    }

    // Hold 判定のために直近数秒分の状態履歴を残しておく。
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
    ReleaseKeyboardDevice(&m_keyboard, m_key, m_keyPrev, &m_keyDeque);
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
    return IsHoldDuration(keyCode, 0.5f);
}

bool KeyBoard::IsHoldDuration(int keyCode, float seconds)
{
    std::size_t holdFrameCount = GetHoldFrameCountForDuration(seconds);
    if (m_keyDeque.size() <= holdFrameCount)
    {
        return false;
    }

    bool isHold = true;
    for (std::size_t i = 0; i < holdFrameCount; ++i)
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

    // 実機の KeyBoard と同じ履歴の持ち方をすることで、
    // Hold や UpFirst も本番と近い形でテストできる。
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
    return IsHoldDuration(keyCode, 0.5f);
}

bool MockKeyBoard::IsHoldDuration(int keyCode, float seconds)
{
    std::size_t holdFrameCount = GetHoldFrameCountForDuration(seconds);
    if (m_keyDeque.size() <= holdFrameCount)
    {
        return false;
    }

    bool isHold = true;
    for (std::size_t i = 0; i < holdFrameCount; ++i)
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
