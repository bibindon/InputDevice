#include "InputDeviceInternal.h"
#include <algorithm>
#include <stdexcept>

namespace InputDevice
{

using namespace Internal;

namespace
{
    const wchar_t* kGamePadTestWindowClassName = L"InputDeviceGamePadTestWindow";
    const int kControlMomentaryMode = 1001;
    const int kControlToggleMode = 1002;
    const int kControlKeyboardMode = 1003;
    const int kControlClear = 1004;
    const int kControlFirstGamePadButton = 2000;

    enum class TestInputMode
    {
        Momentary,
        Toggle,
        Keyboard,
    };

    struct TestButtonDefinition
    {
        const wchar_t* label;
        GamePadButton button;
        int x;
        int y;
        int width;
    };

    const TestButtonDefinition kTestButtons[] =
    {
        { L"X", GAMEPAD_X, 250, 123, 42 },
        { L"A", GAMEPAD_A, 292, 156, 42 },
        { L"B", GAMEPAD_B, 334, 123, 42 },
        { L"Y", GAMEPAD_Y, 292, 90, 42 },
        { L"L1", GAMEPAD_L1, 18, 50, 65 },
        { L"R1", GAMEPAD_R1, 304, 50, 65 },
        { L"L2", GAMEPAD_L2, 90, 50, 65 },
        { L"R2", GAMEPAD_R2, 232, 50, 65 },
        { L"BACK", GAMEPAD_BACK, 145, 105, 65 },
        { L"START", GAMEPAD_START, 145, 140, 65 },
        { L"↑", GAMEPAD_POV_UP, 66, 115, 42 },
        { L"→", GAMEPAD_POV_RIGHT, 105, 148, 42 },
        { L"↓", GAMEPAD_POV_DOWN, 66, 181, 42 },
        { L"←", GAMEPAD_POV_LEFT, 27, 148, 42 },
    };

    const std::size_t kTestButtonCount = sizeof(kTestButtons) / sizeof(kTestButtons[0]);
    HWND g_gamePadTestWindow = nullptr;
    WNDPROC g_originalButtonWindowProc = nullptr;
    TestInputMode g_testInputMode = TestInputMode::Momentary;
    bool g_testButtonDown[kTestButtonCount] = { };

    void SetControlFont(HWND control)
    {
        if (control == nullptr)
        {
            return;
        }

        SendMessage(control, WM_SETFONT,
                    reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
    }

    void UpdateButtonVisual(std::size_t index)
    {
        if (g_gamePadTestWindow == nullptr || kTestButtonCount <= index)
        {
            return;
        }

        HWND buttonWindow = GetDlgItem(
            g_gamePadTestWindow,
            kControlFirstGamePadButton + static_cast<int>(index));
        if (buttonWindow == nullptr)
        {
            return;
        }

        WPARAM state = FALSE;
        if (g_testButtonDown[index])
        {
            state = TRUE;
        }
        SendMessage(buttonWindow, BM_SETSTATE, state, 0);
    }

    void SetTestButtonDown(std::size_t index, bool isDown)
    {
        if (kTestButtonCount <= index)
        {
            return;
        }

        g_testButtonDown[index] = isDown;
        GamePad::SetInjectedButtonDown(kTestButtons[index].button, isDown);
        UpdateButtonVisual(index);
    }

    void ClearTestButtons()
    {
        GamePad::ClearInjectedButtons();
        GamePad::ClearInjectedSticks();
        std::fill(&g_testButtonDown[0], &g_testButtonDown[kTestButtonCount], false);

        for (std::size_t i = 0; i < kTestButtonCount; ++i)
        {
            UpdateButtonVisual(i);
        }
    }

    void SetTestButtonDown(GamePadButton button, bool isDown)
    {
        for (std::size_t i = 0; i < kTestButtonCount; ++i)
        {
            if (kTestButtons[i].button == button)
            {
                SetTestButtonDown(i, isDown);
                return;
            }
        }
    }

    void SetTestInputMode(TestInputMode mode)
    {
        ClearTestButtons();
        g_testInputMode = mode;
    }

    LRESULT CALLBACK TestButtonWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        std::size_t index = static_cast<std::size_t>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

        if (g_testInputMode == TestInputMode::Momentary)
        {
            if (message == WM_LBUTTONDOWN ||
                (message == WM_KEYDOWN && wParam == VK_SPACE))
            {
                SetTestButtonDown(index, true);
            }
            else if (message == WM_LBUTTONUP ||
                     (message == WM_KEYUP && wParam == VK_SPACE) ||
                     message == WM_CAPTURECHANGED)
            {
                SetTestButtonDown(index, false);
            }
        }

        return CallWindowProc(g_originalButtonWindowProc, hWnd, message, wParam, lParam);
    }

    HWND CreateTestControl(const wchar_t* className,
                           const wchar_t* text,
                           DWORD style,
                           int x,
                           int y,
                           int width,
                           int height,
                           int controlId)
    {
        HWND control = CreateWindowExW(
            0,
            className,
            text,
            WS_CHILD | WS_VISIBLE | style,
            x,
            y,
            width,
            height,
            g_gamePadTestWindow,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
            GetModuleHandleW(nullptr),
            nullptr);
        SetControlFont(control);
        return control;
    }

    void CreateTestWindowControls()
    {
        HWND momentaryMode = CreateTestControl(
            L"BUTTON", L"押している間", BS_AUTORADIOBUTTON | WS_GROUP | WS_TABSTOP,
            18, 12, 110, 22, kControlMomentaryMode);
        CreateTestControl(
            L"BUTTON", L"トグルモード", BS_AUTORADIOBUTTON | WS_TABSTOP,
            135, 12, 115, 22, kControlToggleMode);
        CreateTestControl(
            L"BUTTON", L"キーボードモード", BS_AUTORADIOBUTTON | WS_TABSTOP,
            255, 12, 125, 22, kControlKeyboardMode);
        CreateTestControl(
            L"BUTTON", L"すべて解除", BS_PUSHBUTTON | WS_TABSTOP,
            395, 10, 95, 25, kControlClear);

        CreateTestControl(
            L"BUTTON", L"キーボードモードの割り当て", BS_GROUPBOX,
            390, 45, 225, 175, 0);
        CreateTestControl(
            L"STATIC",
            L"左スティック  W A S D\r\n"
            L"右スティック  T F G H\r\n"
            L"十字キー      方向キー\r\n\r\n"
            L"ボタン  I=Y  J=X  K=A  L=B\r\n"
            L"L側     Q=L1  E=L2\r\n"
            L"R側     U=R1  O=R2\r\n"
            L"システム R=START\r\n"
            L"         Y=SELECT/BACK",
            SS_LEFT,
            405, 67, 195, 140, 0);

        SendMessage(momentaryMode, BM_SETCHECK, BST_CHECKED, 0);

        for (std::size_t i = 0; i < kTestButtonCount; ++i)
        {
            const TestButtonDefinition& definition = kTestButtons[i];
            HWND buttonWindow = CreateTestControl(
                L"BUTTON",
                definition.label,
                BS_PUSHBUTTON | WS_TABSTOP,
                definition.x,
                definition.y,
                definition.width,
                28,
                kControlFirstGamePadButton + static_cast<int>(i));
            SetWindowLongPtr(buttonWindow, GWLP_USERDATA, static_cast<LONG_PTR>(i));
            WNDPROC originalProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(
                buttonWindow,
                GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(TestButtonWindowProc)));
            if (g_originalButtonWindowProc == nullptr)
            {
                g_originalButtonWindowProc = originalProc;
            }
        }
    }

    LRESULT CALLBACK GamePadTestWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_CREATE:
            g_gamePadTestWindow = hWnd;
            CreateTestWindowControls();
            return 0;

        case WM_COMMAND:
        {
            int controlId = LOWORD(wParam);
            int notificationCode = HIWORD(wParam);
            if (controlId == kControlMomentaryMode && notificationCode == BN_CLICKED)
            {
                SetTestInputMode(TestInputMode::Momentary);
                return 0;
            }

            if (controlId == kControlToggleMode && notificationCode == BN_CLICKED)
            {
                SetTestInputMode(TestInputMode::Toggle);
                return 0;
            }

            if (controlId == kControlKeyboardMode && notificationCode == BN_CLICKED)
            {
                SetTestInputMode(TestInputMode::Keyboard);
                if (g_inputHWnd != nullptr)
                {
                    SetForegroundWindow(g_inputHWnd);
                }
                return 0;
            }

            if (controlId == kControlClear && notificationCode == BN_CLICKED)
            {
                ClearTestButtons();
                return 0;
            }

            int index = controlId - kControlFirstGamePadButton;
            if (g_testInputMode == TestInputMode::Toggle &&
                notificationCode == BN_CLICKED &&
                0 <= index && static_cast<std::size_t>(index) < kTestButtonCount)
            {
                bool isDown = !g_testButtonDown[index];
                SetTestButtonDown(static_cast<std::size_t>(index), isDown);
                return 0;
            }
            break;
        }

        case WM_ACTIVATE:
            if (LOWORD(wParam) == WA_INACTIVE &&
                g_testInputMode == TestInputMode::Momentary)
            {
                ClearTestButtons();
            }
            break;

        case WM_CLOSE:
            ClearTestButtons();
            ShowWindow(hWnd, SW_HIDE);
            return 0;

        case WM_DESTROY:
            ClearTestButtons();
            g_gamePadTestWindow = nullptr;
            return 0;
        }

        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    void RegisterGamePadTestWindowClass()
    {
        WNDCLASSEXW windowClass = { };
        windowClass.cbSize = sizeof(windowClass);
        windowClass.lpfnWndProc = GamePadTestWindowProc;
        windowClass.hInstance = GetModuleHandleW(nullptr);
        windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
        windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
        windowClass.lpszClassName = kGamePadTestWindowClassName;

        ATOM result = RegisterClassExW(&windowClass);
        if (result == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        {
            throw std::runtime_error("Failed to register the game pad test window class.");
        }
    }
}

bool Internal::IsGamePadTestKeyboardMode()
{
    if (g_gamePadTestWindow == nullptr || !IsWindowVisible(g_gamePadTestWindow))
    {
        return false;
    }

    return g_testInputMode == TestInputMode::Keyboard;
}

void Internal::UpdateGamePadTestKeyboardInput()
{
    if (!IsGamePadTestKeyboardMode())
    {
        return;
    }

    float stickLX = 0.0f;
    float stickLY = 0.0f;
    if (SKeyBoard::IsDown(DIK_A))
    {
        stickLX -= 1.0f;
    }
    if (SKeyBoard::IsDown(DIK_D))
    {
        stickLX += 1.0f;
    }
    if (SKeyBoard::IsDown(DIK_W))
    {
        stickLY += 1.0f;
    }
    if (SKeyBoard::IsDown(DIK_S))
    {
        stickLY -= 1.0f;
    }
    GamePad::SetInjectedStickL(stickLX, stickLY);

    float stickRX = 0.0f;
    float stickRY = 0.0f;
    if (SKeyBoard::IsDown(DIK_F))
    {
        stickRX -= 1.0f;
    }
    if (SKeyBoard::IsDown(DIK_H))
    {
        stickRX += 1.0f;
    }
    if (SKeyBoard::IsDown(DIK_T))
    {
        stickRY += 1.0f;
    }
    if (SKeyBoard::IsDown(DIK_G))
    {
        stickRY -= 1.0f;
    }
    GamePad::SetInjectedStickR(stickRX, stickRY);

    SetTestButtonDown(GAMEPAD_POV_UP, SKeyBoard::IsDown(DIK_UP));
    SetTestButtonDown(GAMEPAD_POV_RIGHT, SKeyBoard::IsDown(DIK_RIGHT));
    SetTestButtonDown(GAMEPAD_POV_DOWN, SKeyBoard::IsDown(DIK_DOWN));
    SetTestButtonDown(GAMEPAD_POV_LEFT, SKeyBoard::IsDown(DIK_LEFT));
    SetTestButtonDown(GAMEPAD_Y, SKeyBoard::IsDown(DIK_I));
    SetTestButtonDown(GAMEPAD_X, SKeyBoard::IsDown(DIK_J));
    SetTestButtonDown(GAMEPAD_A, SKeyBoard::IsDown(DIK_K));
    SetTestButtonDown(GAMEPAD_B, SKeyBoard::IsDown(DIK_L));
    SetTestButtonDown(GAMEPAD_L1, SKeyBoard::IsDown(DIK_Q));
    SetTestButtonDown(GAMEPAD_L2, SKeyBoard::IsDown(DIK_E));
    SetTestButtonDown(GAMEPAD_R1, SKeyBoard::IsDown(DIK_U));
    SetTestButtonDown(GAMEPAD_R2, SKeyBoard::IsDown(DIK_O));
    SetTestButtonDown(GAMEPAD_START, SKeyBoard::IsDown(DIK_R));
    SetTestButtonDown(GAMEPAD_BACK, SKeyBoard::IsDown(DIK_Y));
}

void GamePad::ToggleTestDialog(HWND parentWindow)
{
    if (g_gamePadTestWindow == nullptr)
    {
        RegisterGamePadTestWindowClass();
        g_gamePadTestWindow = CreateWindowExW(
            WS_EX_TOOLWINDOW,
            kGamePadTestWindowClassName,
            L"ゲームパッド入力テスト (F10で閉じる)",
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            650,
            285,
            parentWindow,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr);
        if (g_gamePadTestWindow == nullptr)
        {
            throw std::runtime_error("Failed to create the game pad test window.");
        }
    }

    if (IsWindowVisible(g_gamePadTestWindow))
    {
        ClearTestButtons();
        ShowWindow(g_gamePadTestWindow, SW_HIDE);
        if (parentWindow != nullptr)
        {
            SetForegroundWindow(parentWindow);
        }
        return;
    }

    ShowWindow(g_gamePadTestWindow, SW_SHOW);
    SetForegroundWindow(g_gamePadTestWindow);
}

void Internal::DestroyGamePadTestDialog()
{
    if (g_gamePadTestWindow == nullptr)
    {
        return;
    }

    DestroyWindow(g_gamePadTestWindow);
    g_gamePadTestWindow = nullptr;
}

}
