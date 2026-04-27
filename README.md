# InputDevice

DirectX9 を前提にした、キーボード、マウス、ゲームパッド入力を扱うためのライブラリです。  
DirectInput と XInput の両方に対応していて、サンプルプログラムで各入力の状態を確認できます。

## できること

- キーボードの `IsDown`、`IsDownFirstFrame`、`IsHold`、`IsUpFirstFrame`
- マウスのボタン、座標、移動量、ホイール量の取得
- DirectInput / XInput ゲームパッドのボタン、十字キー、スティックの取得
- `UnifiedInput` による入力の統合

`UnifiedInput` を使うと、たとえば「ゲームパッドの十字キーの上」と「キーボードの上矢印」を同じ入力として扱えます。  
また、`GAMEPAD_A` に `ESC` やマウスのサイドボタンを割り当てる、といった使い方もできます。

## 仕組み

このライブラリは毎フレーム `Update()` を呼ぶ前提で動作します。  
各デバイスの現在フレームと前フレームの状態を保持し、その差分から `FirstFrame` や `UpFirstFrame` を判定しています。

主な構成は次の通りです。

- `KeyBoard` / `SKeyBoard`
  キーボード入力を扱います。`MockKeyBoard` もあり、テスト用の差し替えができます。
- `Mouse`
  マウスボタン、座標、移動量、ホイール量を扱います。
- `GamePad_D`
  DirectInput 版のゲームパッド入力です。
- `GamePad_X`
  XInput 版のゲームパッド入力です。
- `GamePad`
  DirectInput と XInput の違いを意識せずに使うための窓口です。両方有効な場合は XInput を優先します。
- `UnifiedInput`
  キーボード、マウス、ゲームパッドをまとめて扱うためのクラスです。

## 基本的な使い方

```cpp
InputDevice::Initialize(hInstance, hWnd);

while (true)
{
    InputDevice::Update();

    if (InputDevice::SKeyBoard::IsDown(DIK_SPACE))
    {
    }

    if (InputDevice::Mouse::IsDown(InputDevice::MOUSE_LEFT))
    {
    }

    if (InputDevice::GamePad::IsDown(InputDevice::GAMEPAD_A))
    {
    }

    if (InputDevice::UnifiedInput::IsDown(InputDevice::GAMEPAD_POV_UP))
    {
    }
}

InputDevice::Finalize();
```

## サンプルプログラム

`Sample` プロジェクトでは、各入力状態の表示に加えて、`UnifiedInput::GetStickL()` と `GetStickR()` を使ったカメラ操作も確認できます。  
WASD、マウス、ゲームパッドを使って、統合入力の動作を試せます。
