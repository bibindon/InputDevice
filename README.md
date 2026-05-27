# InputDevice

InputDevice は、DirectX9 を前提にしたキーボード、マウス、ゲームパッド入力用のライブラリである。  
DirectInput と XInput の両方に対応し、サンプルプログラムで各入力の状態を確認できる。

## できること

- キーボードの `IsDown`、`IsDownFirstFrame`、`IsHold`、`IsUpFirstFrame`
- マウスのボタン、座標、移動量、ホイール量の取得
- DirectInput / XInput ゲームパッドのボタン、十字キー、スティックの取得
- `UnifiedInput` による入力の統合

`UnifiedInput` を使うと、たとえば「ゲームパッドの十字キーの上」と「キーボードの上矢印」を同じ入力として扱える。  
また、`GAMEPAD_A` に `ESC` やマウスのサイドボタンを割り当てる、といった使い方もできる。

## リモートデスクトップでの動作

このライブラリはマウス移動量の取得に DirectInput を使っている。  
`Mouse::GetDelta()` はカーソル座標の差分ではなく、DirectInput の `DIMOUSESTATE2::lX` / `lY` が返す相対移動量を使用する。

そのため、Windows のリモートデスクトップ上でサンプルプログラムを動かした場合でも、カメラ操作は通常どおり動作する。  
リモートデスクトップ専用の入力モードは用意していないが、サンプルのカメラ操作は DirectInput の相対移動量を使う設計であるため、カーソル座標差に依存する実装よりもリモートデスクトップ環境で破綻しにくい。

## 仕組み

このライブラリは毎フレーム `Update()` を呼ぶ前提で動作する。  
各デバイスの現在フレームと前フレームの状態を保持し、その差分から `FirstFrame` や `UpFirstFrame` を判定する。

主な構成は次の通りである。

- `KeyBoard` / `SKeyBoard`
  キーボード入力を扱う。`MockKeyBoard` もあり、テスト用の差し替えができる。
- `Mouse`
  マウスボタン、座標、移動量、ホイール量を扱う。
- `GamePad_D`
  DirectInput 版のゲームパッド入力である。
- `GamePad_X`
  XInput 版のゲームパッド入力である。
- `GamePad`
  DirectInput と XInput の違いを意識せずに使うための窓口である。両方有効な場合は XInput を優先する。
- `UnifiedInput`
  キーボード、マウス、ゲームパッドをまとめて扱うためのクラスである。

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

`Sample` プロジェクトでは、各入力状態の表示に加えて、`UnifiedInput::GetStickL()` と `GetStickR()` を使ったカメラ操作も確認できる。  
WASD、マウス、ゲームパッドを使って、統合入力の動作を試せる。
