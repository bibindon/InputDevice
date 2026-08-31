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
    const int kControlClear = 1003;
    const int kControlFirstGamePadButton = 2000;

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
    bool g_toggleMode = false;
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
        std::fill(&g_testButtonDown[0], &g_testButtonDown[kTestButtonCount], false);

        for (std::size_t i = 0; i < kTestButtonCount; ++i)
        {
            UpdateButtonVisual(i);
        }
    }

    LRESULT CALLBACK TestButtonWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        std::size_t index = static_cast<std::size_t>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

        if (!g_toggleMode)
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
            L"BUTTON", L"すべて解除", BS_PUSHBUTTON | WS_TABSTOP,
            270, 10, 95, 25, kControlClear);

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
                g_toggleMode = false;
                ClearTestButtons();
                return 0;
            }

            if (controlId == kControlToggleMode && notificationCode == BN_CLICKED)
            {
                g_toggleMode = true;
                ClearTestButtons();
                return 0;
            }

            if (controlId == kControlClear && notificationCode == BN_CLICKED)
            {
                ClearTestButtons();
                return 0;
            }

            int index = controlId - kControlFirstGamePadButton;
            if (g_toggleMode && notificationCode == BN_CLICKED &&
                0 <= index && static_cast<std::size_t>(index) < kTestButtonCount)
            {
                bool isDown = !g_testButtonDown[index];
                SetTestButtonDown(static_cast<std::size_t>(index), isDown);
                return 0;
            }
            break;
        }

        case WM_ACTIVATE:
            if (LOWORD(wParam) == WA_INACTIVE && !g_toggleMode)
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
            410,
            270,
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
