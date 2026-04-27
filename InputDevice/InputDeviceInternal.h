#pragma once

#include "InputDevice.h"
#include <Xinput.h>

namespace InputDevice
{
namespace Internal
{

extern LPDIRECTINPUT8 g_directInput;
extern HWND g_inputHWnd;
extern bool g_keyboardOwnedByLibrary;
extern LPDIRECTINPUTDEVICE8 g_mouse;
extern DIMOUSESTATE2 g_mouseState;
extern DIMOUSESTATE2 g_mousePrevState;
extern MousePosition g_mousePosition;
extern MousePosition g_mousePrevPosition;
extern MousePosition g_mouseDelta;
extern bool g_mouseCursorVisible;
extern std::deque<std::vector<BYTE>> g_mouseButtonDeque;
extern LPDIRECTINPUTDEVICE8 g_gamePad;
extern DIJOYSTATE2 g_gamePadState;
extern DIJOYSTATE2 g_gamePadPrevState;
extern std::deque<std::vector<BYTE>> g_gamePadButtonDeque;
extern std::deque<DWORD> g_gamePadPOVDeque;
extern ULONGLONG g_lastGamePadSearchTime;
extern XINPUT_STATE g_gamePadXState;
extern XINPUT_STATE g_gamePadXPrevState;
extern std::deque<std::vector<BYTE>> g_gamePadXButtonDeque;
extern bool g_gamePadXConnected;
extern GamePad_D g_gamePadD;
extern GamePad_X g_gamePadX;

extern const std::size_t kMouseButtonCount;
extern const std::size_t kGamePadButtonCount;
extern const std::size_t kGamePadXButtonStateCount;
extern const std::size_t kHoldFrameCount;
extern const std::size_t kInputHistoryFrameCount;
extern const LONG kGamePadAxisMin;
extern const LONG kGamePadAxisMax;
extern const float kGamePadStickDeadZone;
extern const BYTE kGamePadXTriggerThreshold;
extern const ULONGLONG kGamePadSearchIntervalMilliseconds;

bool IsValidMouseButtonIndex(char key);
bool GetMouseWindowCenterScreenPosition(POINT* centerPosition);
void ApplyMouseCursorVisible(bool isVisible);
void CenterMouseCursorInWindow();
void UpdateMousePosition();
bool IsMouseCursorInWindow();

bool IsValidGamePadButtonIndex(GamePadButton button);
bool IsValidGamePadXButtonStateIndex(GamePadButton button);
bool IsGamePadPOVButton(GamePadButton button);
int GetUnifiedInputKeyCode(GamePadButton button);

float ClampFloat(float value, float minValue, float maxValue);
float NormalizeGamePadAxis(LONG axis);
float ApplyGamePadStickDeadZone(float value);
GamePadStick CreateStickFromFloatAxis(float x, float y);
GamePadStick CreateGamePadStick(LONG xAxis, LONG yAxis);
LONG ConvertXInputAxisToGamePadAxis(SHORT axis);

bool IsGamePadXButtonPressed(GamePadButton button, const XINPUT_STATE& state);
IGamePad* GetActiveGamePad();
void SetGamePadXButtonState(std::vector<BYTE>* buttonState, GamePadButton button);
void SetGamePadAxisRange(DWORD objectOffset);
void ReleaseGamePadDDevice();

bool IsGamePadPOVPressed(DWORD pov, DWORD minValue, DWORD maxValue);
bool IsGamePadCurrentPOVPressed(DWORD minValue, DWORD maxValue);
bool IsGamePadPrevPOVPressed(DWORD minValue, DWORD maxValue);
bool IsGamePadPOVFirstFrame(DWORD minValue, DWORD maxValue);
bool IsGamePadPOVHold(DWORD minValue, DWORD maxValue);
bool IsGamePadCurrentPOVButtonPressed(GamePadButton button);
bool IsGamePadPOVButtonFirstFrame(GamePadButton button);
bool IsGamePadPOVButtonHold(GamePadButton button);

BOOL CALLBACK EnumGamePadCallback(const DIDEVICEINSTANCE* instance, VOID* context);

}
}
