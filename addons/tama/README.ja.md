<p align="center">
  <img src="logo-small.png" />
</p>

# Tama for Godot
[![Discord](https://img.shields.io/discord/1509716856489906337?style=flat&logo=discord&logoColor=white&label=discord&labelColor=5865F2)](https://discord.gg/7qs72jGrub)

Tamaは、独自の弾幕パターン定義言語「TamaScript」を搭載した、弾幕STG制作のためのシンプルなフレームワークです。

アドオン内の `example` フォルダにサンプルのTamaScriptファイルがあります。`example.tscn` を実行すると、TamaScriptを貼り付けてすぐに動作確認できるテキストフィールドが表示されます。

## クイックスタート
1. このリポジトリの `addons` フォルダをプロジェクトにドラッグ＆ドロップし、「プロジェクト設定 → プラグイン」から有効にします。
2. `TamaBullet` ノード（または `TamaBullet` を継承したノード）から弾のシーンを作成します。
3. `TamaBulletRegistry` リソースを作成し、弾を `entries` と `default_bullet` に追加します。`entries` に追加する際は弾にタイプ名を付ける必要があります。
4. プロジェクト設定の「Tama → Scripts Path」に、TamaScriptファイルを保存するフォルダを設定します。デフォルトは `res://tamascripts` です。
5. TamaScriptファイルを作成し、先ほど設定したフォルダに保存します。
6. シーンに `TamaEmitter` を追加し、`script_filename` にTamaScriptファイルの名前を設定します。
7. `TamaManager` を設定します:
    ```gdscript
    # ステップ3で作成したレジストリ
    TamaManager.registry(load("res://my_bullet_registry.tres"))
    ```
8. `TamaEmitter` の `start()` を呼び出してTamaScriptを実行します。
9. （任意）カスタムの `TamaContext` を設定することで、独自のGDScript関数をTamaScriptに公開できます:
    ```
    # example_tama_context.gd
    extends TamaContext
    class_name MyTamaContext

    func some_func()
        # ここに戻り値を返す処理

    # main.gd
    TamaManager.context = MyTamaContext.new()

    # example.tama
    main
        fire
            dir abs some_func()
            spd 200
    ```

## TamaScript 構文

TamaScriptはインデントベースの言語です。`#` でコメントを開始します。

### 構造

```
main              ← エントリーポイント。エミッターが開始したときに実行される
    repeat
        fire myfire(200)
        wait 0.3

fire myfire(spd_) ← 名前付きfire定義
    dir aim 0
    spd spd_

act circle(n)     ← 名前付きアクションシーケンス
    repeat n i
        fire
            dir abs (360/n)*i
            spd 200

bullet tracker    ← 名前付きbullet定義
    type enemy    ← TamaBulletRegistryで "enemy" を検索する
    act
        repeat
            chdir
                dir aim 0
                over 0.5
            wait 0.5
```

### `fire` ブロックのプロパティ

| 文 | デフォルト | 説明 |
|---|---|---|
| `dir [aim\|abs\|rel\|seq] EXPR` | `aim` | 弾の方向（度数）。`aim` = プレイヤーへの向き＋オフセット、`abs` = ワールド角度、`rel` = スポウナーの角度に対する相対、`seq` = 最後に発射した角度に対する相対。 |
| `speed [abs\|rel\|seq] EXPR` | `abs` | 弾の速度。`rel`/`seq` は最後に発射した速度に加算。 |
| `offset EXPR` | — | 弾のローカル軸方向へのスポーンオフセット。 |
| `offset` *(ブロック)* | — | 軸ごとのスポーンオフセット。デフォルト修飾子は `rel`（弾の角度で回転）。`abs`/`seq` はワールド空間でスポウナー位置に加算。 |
| `pos` *(ブロック)* | — | スポーン位置を直接指定。デフォルト修飾子は `abs`（ワールド座標）。`rel` はスポウナー位置に加算。`offset` より優先される。 |
| `bullet NAME` | レジストリのデフォルト | スポーンする弾の種類。 |

```
fire
    dir abs 90
    speed 150
    pos
        x abs 500
        y rel 0
    bullet my_bullet(arg1)
```

### `act` ブロックの文

| 文 | 説明 |
|---|---|
| `wait EXPR` | N秒間停止。 |
| `waitf EXPR` | N物理フレーム停止。 |
| `repeat [N] [i]` | N回ループ（Nを省略すると無限ループ）。`i` = 1始まりのインデックス。 |
| `fire NAME` / `fire` *(インライン)* | 弾をスポーンする。 |
| `act NAME` / `act` *(インライン)* | アクトを実行する（ブロッキング）。 |
| `async act …` | アクトをブロッキングなしで実行する。 |
| `chdir` / `chspd` / `accel` | この弾にトランジションコマンドを送る。 |
| `vanish` | この弾のアクトを停止し、弾を消去する。 |

### `bullet` ブロックの文

| 文 | 説明 |
|---|---|
| `type NAME` | 弾のシーンタイプ — `TamaBulletRegistry` で `NAME` を検索する。省略するとレジストリのデフォルトを使用。 |
| `emt NAME` / `emt` *(インライン)* | 弾の `act` と並列で動作する発射エミッターを付与する。 |
| `act NAME` / `act` *(インライン)* | 弾がスポーンした後に実行する動作。 |

### 定義を引数として渡す

`fire`・`act`・`bullet` の定義は引数として渡して、受け取った側から呼び出すことができます。渡す際に引数をあらかじめバインドすることも可能です:

```
act x_way(n, f, spd_)
    repeat n i
        fire f          ← f として渡されたfire定義を呼び出す
        wait 0.05

main
    act x_way(8, spread)              ← fire定義 "spread" を名前で渡す
    act x_way(8, spread(45), 200)    ← 45をspreadの第1引数としてあらかじめバインド
```

### 弾の方向・速度のトランジション（bulletの `act` 内）

```
chdir               ← 方向を変更
    dir aim 0
    over 1.0        ← トランジション時間（秒）

chspd
    spd abs 400
    over 2.0

accel               ← ワールド軸加速度
    x 0
    y 50
    over 1.5
```

### `export` と `include`

```
export num speed 200    ← インスペクターにfloat型フィールドを公開する
export str dir_mode aim ← string型フィールドを公開する（aim/abs/rel/seqなどに使用）

include builtin         ← 別の.tamaファイルからfire/act/bulletの定義をマージする
```

## スペシャルサンクス
[@icons](https://github.com/Voxybuns/at-icons)
