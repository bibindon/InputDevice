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

        if (ret == DIERR_INPUTLOST)
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
        else if (ret == DIERR_NOTACQUIRED || ret == DIERR_OTHERAPPHASPRIO)
        {
            return;
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
    // 今回は「30フレーム連続で押されている」を Hold としている。
    // 実時間ではなくフレーム基準なので、FPS が変わると体感時間も変わる。
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
