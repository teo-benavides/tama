<p align="center">
  <img src="logo-small.png" />
</p>

# Tama for Godot
[![Discord](https://img.shields.io/discord/1509716856489906337?style=flat&logo=discord&logoColor=white&label=discord&labelColor=5865F2)](https://discord.gg/7qs72jGrub)

[TamaScript用のVSCode拡張機能をダウンロード](https://marketplace.visualstudio.com/items?itemName=teo-benavides.tamascript)

Tamaは、独自の弾幕定義言語「TamaScript」を搭載した、弾幕STG制作のためのシンプルなフレームワークです。

アドオン内の `example` フォルダにサンプルのTamaScriptファイルがあります。`example.tscn` を実行すると、TamaScriptを貼り付けてすぐに動作確認できるテキストフィールドが表示されます。

## クイックスタート
1. このリポジトリの `addons` フォルダをプロジェクトにドラッグ＆ドロップし、「プロジェクト設定 → プラグイン」から有効にします。
2. `TamaBullet` ノード（または `TamaBullet` を継承したノード）から弾のシーンを作成します。
3. `TamaBulletRegistry` リソースを作成し、`default_scene_bullet` に先ほど作成したシーンを設定します。
4. プロジェクト設定の「Tama → Scripts Path」に、TamaScriptファイルを保存するフォルダを設定します。デフォルトは `res://tamascripts` です。
5. TamaScriptファイルを作成し、先ほど設定したフォルダに保存します。
6. シーンに `TamaEmitter` を追加し、`script_filename` にTamaScriptファイルの名前を設定します。
7. `TamaManager` を設定します:
    ```gdscript
    TamaManager.set_registry(load("res://my_bullet_registry.tres"))
    TamaManager.load_scripts()
    TamaManager.set_player_position($Player.global_position)  # 毎フレーム更新する
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

## TamaAnimatedTexture

`TamaAnimatedTexture` は、すべてのサーバーオブジェクトコンフィグで使用されるテクスチャ型です。アニメーションフレーム・再生速度・ブレンドモードを保持し、そのタイプの全オブジェクトに適用されます。

| プロパティ | 型 | デフォルト | 説明 |
|---|---|---|---|
| `frame_count` | `int` | `0` | フレーム数。`1` にすると静止テクスチャになる。 |
| `fps` | `float` | `60` | アニメーション再生速度。`0` 以下でアニメーション無効。 |
| `blend_mode` | `enum` | `Mix` | オブジェクトタイプの合成方法：**Mix**・**Add**・**Subtract**・**Multiply**・**Premultiplied Alpha**。タイプ単位で適用される。 |
| `frame_N/texture` | `Texture2D` | — | フレームNのテクスチャ（`frame_count` > 0のときインスペクターに表示）。 |
| `frame_N/duration` | `int` | `1` | 次のフレームに進む前にこのフレームを表示するアニメーションフレーム数。 |

## サーバー弾

サーバー弾は `RenderingServer`（MultiMesh）を使ってレンダリングします。弾ごとにシーンノードを作成せず、コリジョンは毎物理フレームにプレイヤー位置に対してCPUで円形判定を行います。数百〜数千発の弾を同時に扱う高密度な弾幕パターンに適しています。

### セットアップ

1. `TamaBulletRegistry` の `objects` ディクショナリを開き、キーをタイプ名、値を新しい `TamaServerBulletConfig` としてエントリを追加します。
2. `TamaServerBulletConfig` の設定を変更します（下記プロパティ参照）。
3. （任意）「Default to Server Bullets」にチェックを入れると、`bul` ブロックが指定されていないときにこのタイプがデフォルトになります。

### TamaServerBulletConfig のプロパティ

| プロパティ | 型 | デフォルト | 説明 |
|---|---|---|---|
| `texture` | `TamaAnimatedTexture` | — | 弾のアニメーションテクスチャ。1フレーム = 静止スプライト。ブレンドモードもここで設定。 |
| `auto_rect` | `bool` | `true` | 最初のフレームのピクセルサイズからスプライトの矩形を自動計算する。 |
| `rect` | `Rect2` | `(-8,-8,16,16)` | スプライトの描画矩形（`auto_rect` がオフのときに使用）。 |
| `texture_scale` | `Vector2` | `(1,1)` | スプライトの見た目にのみ適用するスケール。 |
| `shape_radius` | `float` | `6` | CPUプレイヤー判定に使うコリジョン円の半径（ピクセル）。 |
| `rotates` | `bool` | `true` | スプライトが弾の角度に合わせて回転するかどうか。 |
| `face_velocity` | `bool` | `true` | スプライトが移動方向を向くかどうか（`rotates` が必要）。 |
| `pool_size` | `int` | `1000` | このタイプの最大同時弾数。超過したスポーンは無視される。 |
| `out_of_bounds_margin` | `float` | `50` | 弾がリサイクルされるまでの画面外余白（ピクセル）。 |
| `spawn_delay` | `int` | `0` | 弾が動き始める前に停止するフレーム数（物理フレーム）。この間スポーンアニメーションが再生され、コリジョンも無効になる。TamaScript の `delay` で上書き可能。 |
| `spawn_texture` | `TamaAnimatedTexture` | — | スポーンエフェクト用のアニメーションテクスチャ（弾の上に描画）。独自のブレンドモードをサポート。未設定の場合は `texture` の1枚目にフォールバック。 |
| `starting_spawn_scale` | `float` | `2.0` | ディレイ最初のフレームにおけるスポーンスプライトのスケール。ディレイ終了時に `1.0` へ補間される。 |
| `starting_spawn_opacity` | `float` | `0.0` | ディレイ最初のフレームにおけるスポーンスプライトの不透明度。ディレイ終了時に `1.0` へ補間される。 |

### コリジョン検出

`TamaManager.bullet_hit` に接続します。`shape_radius` + `TamaManager.player_hitbox_radius` が `TamaManager.player_position` と重なったときに発火します:

```gdscript
func _ready():
    TamaManager.bullet_hit.connect(_on_bullet_hit)

func _on_bullet_hit(bullet: TamaServerBullet) -> void:
    TamaManager.destroy_server_bullet(bullet)
```

`TamaServerBullet` が公開するプロパティ: `position`、`angle`、`speed`、`speed_x`、`speed_y`、`active`。

### 制限事項

サーバー弾では以下のTamaScript機能は**サポートされていません**:

- **`emt`** — 弾に発射エミッターをアタッチする機能はシーンノードが必要なため、サーバー弾では動作しません。

## ストレートレーザー

ストレートレーザーは、固定位置・固定角度のビーム型オブジェクトです。**ディレイ**（警告線、コリジョンなし）→ **展開**（幅が広がる）→ **アクティブ**（全幅）→ **フェード**（消滅）の4フェーズを経ます。

### セットアップ

`TamaBulletRegistry` の `objects` ディクショナリに `TamaServerStraightLaserConfig` エントリを追加します。

### TamaServerStraightLaserConfig のプロパティ

| プロパティ | 型 | デフォルト | 説明 |
|---|---|---|---|
| `width` | `float` | `20` | 最大展開時のビーム幅（ピクセル）。 |
| `length` | `float` | `1000` | スポーン位置からのビーム長（ピクセル）。 |
| `texture` | `TamaAnimatedTexture` | — | ビームに貼り付けるアニメーションテクスチャ。ブレンドモードもここで設定。 |
| `tile_x` | `bool` | `false` | ビームの長さ方向にテクスチャをタイリングする（オフの場合は引き伸ばし）。 |
| `tile_y` | `bool` | `false` | ビームの幅方向にテクスチャをタイリングする（オフの場合は引き伸ばし）。 |
| `base_texture` | `TamaAnimatedTexture` | — | スポーン位置を中心に描画するオプションテクスチャ（発射フラッシュなど）。 |
| `delay_frames` | `int` | `120` | 1ピクセルの警告線を表示するフレーム数。コリジョンなし。 |
| `expand_frames` | `int` | `10` | 1pxから全幅へ展開するフレーム数。コリジョン有効。 |
| `duration_frames` | `int` | `120` | 全幅を維持するフレーム数。コリジョン有効。 |
| `fade_frames` | `int` | `30` | フェードアウトするフレーム数。コリジョンなし。 |
| `pool_size` | `int` | `1000` | このタイプの最大同時レーザー数。 |

### コリジョン検出

```gdscript
func _ready():
    TamaManager.straight_laser_hit.connect(_on_laser_hit)

func _on_laser_hit(laser: TamaServerLaser) -> void:
    pass  # プレイヤーヒット処理
```

## 曲線レーザー

曲線レーザーは毎フレーム空間を移動し、リボン状のトレイルとして描画されます。トレイルは先端と末端が細くなる三角形ストリップです。コリジョンはトレイル全体のすべてのセグメントに対して判定されます。

### セットアップ

`TamaBulletRegistry` の `objects` ディクショナリに `TamaServerCurvedLaserConfig` エントリを追加します。

### TamaServerCurvedLaserConfig のプロパティ

| プロパティ | 型 | デフォルト | 説明 |
|---|---|---|---|
| `width` | `float` | `20` | リボン幅（ピクセル）。 |
| `length` | `int` | `30` | 保持するトレイルノード数。大きいほどトレイルが長くなる。 |
| `texture` | `TamaAnimatedTexture` | — | リボンに適用するアニメーションテクスチャ（UV: トレイル長方向 0→1）。ブレンドモードもここで設定。 |
| `pool_size` | `int` | `1000` | このタイプの最大同時レーザー数。 |
| `out_of_bounds_margin` | `float` | `50` | レーザーの先端がリサイクルされるまでの画面外余白（ピクセル）。 |

### コリジョン検出

```gdscript
func _ready():
    TamaManager.curved_laser_hit.connect(_on_curved_laser_hit)

func _on_curved_laser_hit(laser: TamaServerCurvedLaser) -> void:
    pass  # プレイヤーヒット処理
```

## TamaManager リファレンス

`TamaManager` はシングルトンです。主なプロパティ:

| プロパティ / シグナル | 型 | 説明 |
|---|---|---|
| `registry` | `TamaBulletRegistry` | 使用するオブジェクトレジストリ。エミッターの `start()` より前に設定します。 |
| `player_position` | `Vector2` | `dir aim` とコリジョン判定に使うプレイヤーのワールド座標。毎フレーム更新してください。 |
| `player_hitbox_radius` | `float` | CPUコリジョン判定で使うプレイヤーヒットボックスの半径（ピクセル）。デフォルト `3`。 |
| `spawn_parent` | `NodePath` | シーン弾の親ノード。デフォルトはシーンのルート。 |
| `context` | `TamaContext` | GDScript関数をTamaScriptに公開するカスタムコンテキスト。 |
| `global_out_of_bounds_margin` | `float` | 0以上のとき、すべてのタイプのコンフィグごとの余白を上書きします。デフォルト `-1`（無効）。 |
| `bullet_count` | `int` *（読み取り専用）* | 全タイプのアクティブなサーバーオブジェクトの合計数。 |
| `bullet_hit(bullet)` | シグナル | サーバー弾がプレイヤーヒットボックスと重なったときに発火。 |
| `straight_laser_hit(laser)` | シグナル | ストレートレーザーのビームがプレイヤーヒットボックスと重なったときに発火（展開・アクティブフェーズのみ）。 |
| `curved_laser_hit(laser)` | シグナル | 曲線レーザーのトレイルのいずれかのセグメントがプレイヤーヒットボックスと重なったときに発火。 |

主なメソッド:

| メソッド | 説明 |
|---|---|
| `get_server_bullet_pool()` | `TamaServerBulletPool` ノードを返します。 |
| `get_laser_pool()` | ストレートレーザー用の `TamaServerLaserPool` ノードを返します。 |
| `get_curved_laser_pool()` | 曲線レーザー用の `TamaServerCurvedLaserPool` ノードを返します。 |
| `register_bullet(type, scene)` | レジストリの `scene_bullets` 設定に代わる命令型API。 |
| `register_server_bullet(type, config)` | レジストリの `objects` 設定に代わる命令型API。 |
| `destroy_server_bullet(bullet)` | サーバー弾をリサイクルします。`bullet_hit` ハンドラ内で呼び出せます。 |
| `recycle_all()` | アクティブなすべての弾・レーザーを即時リサイクルします。 |
| `load_scripts(path)` | 指定パス（省略時はプロジェクト設定のパス）からTamaScriptファイルをロードします。 |

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
| `rotspd [abs\|rel\|seq] EXPR` | `0`（回転なし）| 弾の初期回転速度（度/秒）。毎フレーム弾の角度に累積される。`rel`/`seq` は最後に発射した回転速度に加算。 |
| `offset EXPR` | — | 弾のローカル軸方向へのスポーンオフセット。 |
| `offset` *(ブロック)* | — | 軸ごとのスポーンオフセット。デフォルト修飾子は `rel`（弾の角度で回転）。`abs`/`seq` はワールド空間でスポウナー位置に加算。 |
| `pos` *(ブロック)* | — | スポーン位置を直接指定。デフォルト修飾子は `abs`（ワールド座標）。`rel` はスポウナー位置に加算。`offset` より優先される。 |
| `bullet NAME` | レジストリのデフォルト | スポーンする弾の種類。 |
| `delay EXPR` | — | *（サーバー弾のみ）* この fire ブロック限定で弾タイプの `spawn_delay` を上書きする。`EXPR` = スポーンアニメーション再生中に停止する物理フレーム数。 |

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
| `repeat [N] [i]` | N回ループ（Nを省略すると無限ループ）。`i` = 0始まりのインデックス。 |
| `repeatf [N] [i]` | 毎物理フレーム1回ブロックを実行する（同期処理）。`N` なし：無限ループ（以降の文は実行されない）。`N` あり：N回実行後に継続する。`i` = 0始まりのインデックス。 |
| `while COND` | `COND` が0以外の間ループする。 |
| `if COND` / `elif COND` / `else` | 条件分岐。 |
| `var NAME EXPR` | ローカル変数を宣言する。 |
| `NAME EXPR` | 既存の変数を再代入する（変更は親スコープに伝播する）。 |
| `fire NAME` / `fire` *(インライン)* | 弾をスポーンする。 |
| `act NAME` / `act` *(インライン)* | アクトを実行する（ブロッキング）。 |
| `async act …` | アクトをブロッキングなしで実行する。 |
| `chdir` / `chspd` / `chrotspd` / `chpos` / `accel` | この弾にトランジションコマンドを送る。`over` を省略するか `0` にすると即時適用される。 |
| `vanish` | この弾のアクトを停止し、弾を消去する。 |
| `break` | 最も内側の `repeat`・`repeatf`・`while` ループを抜ける。 |

### `bullet` ブロックの文

| 文 | 説明 |
|---|---|
| `type NAME` | 弾のタイプ — `TamaBulletRegistry` で `NAME` を検索する。省略するとレジストリのデフォルトを使用。 |
| `emt NAME` / `emt` *(インライン)* | 弾の `act` と並列で動作する発射エミッターを付与する。**サーバー弾では非サポート。** |
| `mvmt` *(ブロック)* | 毎物理フレーム再評価される位置式。`abs` = ワールド座標；`rel` = スポーン位置からのオフセット。 |
| `act NAME` / `act` *(インライン)* | 弾がスポーンした後に実行する動作。 |
| `bounces [N] [x\|y]` | 弾が画面の境界に当たったとき、消えずに反射させる。`N` = 最大反射回数（省略または `-1` で無限）。`x` = 左右の壁のみ；`y` = 上下の壁のみ；軸を省略するとすべての境界に適用。最後の反射後は通常通り画面外で消える。 |

```
bullet wall_bouncer
    bounces 3       ← すべての境界で最大3回反射し、その後画面外へ

bullet side_bouncer
    bounces x       ← 左右の壁で無限に反射

bullet finite_y
    bounces 2 y     ← 上下の壁で2回反射し、その後通常通り画面外へ
```

### 変数と制御フロー

`var NAME EXPR` は現在のブロックにスコープされた変数を宣言します。`var` なしの `NAME EXPR` は既存の変数を再代入し、変更は親スコープに伝播します。`true` と `false` は有効な値です（それぞれ `1.0` と `0.0` に相当）。

```
main
    var count 8
    var speed 200
    while count > 0
        if count > 4
            fire
                dir aim 0
                spd speed
        else
            fire
                dir aim 45
                spd speed * 0.5
        count count - 1    ← 再代入（var キーワード不要）
        wait 0.1
```

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

### 弾の方向・速度・位置のトランジション（bulletの `act` 内）

`over` はすべての文で省略可能で、デフォルトは `0` です。`over` が `0` の場合、トゥイーンを使わず即時適用されます。

```
chdir               ← 方向を変更（即時 — over なし）
    dir aim 0

chdir               ← 方向を変更（トゥイーン）
    dir abs 90
    over 1.0        ← トランジション時間（秒）

chspd
    spd abs 400
    over 2.0

chrotspd            ← 回転速度を変更
    rotspd abs 90   ← 90度/秒（時計回り）
    over 0.5

chpos               ← 位置を変更
    x abs 500
    y abs 300
    over 1.5

accel               ← ワールド軸加速度
    x 0
    y 50
    over 1.5
```

### `export` と `include`

```
export num speed 200     ← インスペクターにfloat型フィールドを公開する
export str dir_mode aim  ← string型フィールドを公開する（aim/abs/rel/seqなどに使用）
export bool enabled true ← bool型フィールドを公開する（インスペクターにチェックボックスとして表示）

include builtin          ← 別の.tamaファイルからfire/act/bulletの定義をマージする
```

## 予定している機能
### TamaScript
- 文字列（`"` で囲む）
- 小さなサンプルゲーム
- ドキュメントコメント
- ノードグラフ式スクリプトエディター

## スペシャルサンクス
[@icons](https://github.com/Voxybuns/at-icons)
