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

## サーバー弾

サーバー弾は、`RenderingServer`（MultiMesh）と`PhysicsServer2D`を直接使用する高パフォーマンスな弾タイプです。弾ごとにシーンノードを作成しないため、数千発の弾を同時に扱う高密度な弾幕パターンに適しています。

### セットアップ

1. `TamaBulletRegistry` の「Default Server Bullet」をクリックし、新しい `TamaServerBulletConfig` を作成します。
2. 作成した `TamaServerBulletConfig` をクリックして設定を変更します（下記プロパティ参照）。
3. 「Default to Server Bullets」にチェックを入れます。

### TamaServerBulletConfig のプロパティ

| プロパティ | 型 | デフォルト | 説明 |
|---|---|---|---|
| `frames` | `Array[Texture2D]` | `[]` | アニメーションフレーム。1枚 = 静止スプライト。 |
| `fps` | `float` | `0` | アニメーション再生速度。`0` = アニメーションなし。 |
| `auto_rect` | `bool` | `true` | 最初のフレームのサイズからスプライトの矩形を自動計算する。オフにすると `rect` を手動設定できる。 |
| `rect` | `Rect2` | `(-8,-8,16,16)` | スプライトの描画矩形（`auto_rect` がオフのときに使用）。 |
| `texture_scale` | `Vector2` | `(1,1)` | スプライトに適用するスケール。 |
| `shape_radius` | `float` | `6` | コリジョン円の半径（ピクセル）。 |
| `collision_layer` | `int` | `1` | 物理コリジョンレイヤー。 |
| `collision_mask` | `int` | `2` | 物理コリジョンマスク。 |
| `rotates` | `bool` | `true` | スプライトが弾の角度に合わせて回転するかどうか。 |
| `face_velocity` | `bool` | `true` | スプライトが移動方向を向くかどうか（`rotates` が必要）。 |
| `pool_size` | `int` | `1000` | このタイプの最大同時弾数。プールが満杯のときは新規スポーンを無視する。 |
| `out_of_bounds_margin` | `float` | `50` | 弾がリサイクルされるまでの画面外余白（ピクセル）。 |
| `spawn_delay` | `int` | `0` | 弾が動き始める前に停止するフレーム数（物理フレーム）。この間スポーンアニメーションが再生される。`0` = ディレイなし（TamaScript の `delay` で上書き可能）。 |
| `spawn_texture` | `Texture2D` | `null` | スポーンアニメーションに使用するテクスチャ。未設定の場合は `frames` の1枚目にフォールバック。 |
| `starting_spawn_scale` | `float` | `2.0` | スポーンアニメーション開始時（t=0）のスケール。ディレイ終了時に 1.0 へ補間される。 |
| `starting_spawn_opacity` | `float` | `0.0` | スポーンアニメーション開始時（t=0）の不透明度。ディレイ終了時に 1.0 へ補間される。 |

### コリジョン検出

`TamaManager` の `bullet_hit` シグナルに接続します:

```gdscript
TamaManager.bullet_hit.connect(_on_bullet_hit)

func _on_bullet_hit(bullet: TamaServerBullet, body_instance_id: int):
    if body_instance_id == get_instance_id():
        # ヒット処理...
    TamaManager.destroy_server_bullet(bullet)
```

`TamaServerBullet` が公開するプロパティ: `position`、`angle`、`speed`、`speed_x`、`speed_y`、`active`。

### 制限事項

サーバー弾では以下のTamaScript機能は**サポートされていません**:

- **`emt`** — 弾に発射エミッターをアタッチする機能はシーンノードが必要なため、サーバー弾では動作しません。

## TamaManager リファレンス

`TamaManager` はシングルトンです。主なプロパティ:

| プロパティ / シグナル | 型 | 説明 |
|---|---|---|
| `registry` | `TamaBulletRegistry` | 使用する弾レジストリ。エミッターの `start()` より前に設定します。 |
| `player_position` | `Vector2` | `dir aim` の計算に使うプレイヤーのワールド座標。毎フレーム更新してください。 |
| `spawn_parent` | `NodePath` | シーン弾の親ノード。デフォルトは現在のシーンのルート。 |
| `context` | `TamaContext` | GDScript関数をTamaScriptに公開するカスタムコンテキスト。 |
| `global_out_of_bounds_margin` | `float` | 0以上のとき、すべてのサーバー弾のコンフィグごとの余白を上書きします。デフォルト `-1`（無効）。 |
| `bullet_count` | `int` *（読み取り専用）* | 全タイプのアクティブな弾の合計数。 |
| `bullet_hit(bullet, body_id)` | シグナル | サーバー弾のコリジョンエリアが物理ボディと重なったときに発火。 |

主なメソッド:

| メソッド | 説明 |
|---|---|
| `get_server_bullet_pool()` | `TamaServerBulletPool` ノードを返します（手動で `recycle()` を呼ぶ際などに使用）。 |
| `register_bullet(type, scene)` | レジストリの `scene_bullets` 設定に代わる命令型API。 |
| `register_server_bullet(type, config)` | レジストリの `server_bullets` 設定に代わる命令型API。 |
| `load_scripts(path)` | 指定パス（省略時はプロジェクト設定のパス）からTamaScriptファイルをリロードします。 |

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
