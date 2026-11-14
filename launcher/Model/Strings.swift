// SPDX-License-Identifier: GPL-2.0-or-later

/// The launcher's text in Japanese and Simplified Chinese, by the English source text
/// (launcher/Model/Localization.swift). "%@" is a value, in the English order; "%1$@", "%2$@"
/// where a translation needs another order. launcher/Tests checks that every L("...") in the
/// sources and every label of the settings and bindings tables has an entry here.
enum Strings {
    static let table: [String: (ja: String, zh: String)] = [
        // Main window
        "General":
            (ja: "一般",
             zh: "常规"),
        "Advanced":
            (ja: "詳細",
             zh: "高级"),
        "Controls":
            (ja: "操作",
             zh: "操作"),
        "Mods & Cheats…":
            (ja: "Mod とチート…",
             zh: "模组与作弊…"),
        "Mods & Cheats":
            (ja: "Mod とチート",
             zh: "模组与作弊"),
        "Show launcher every time":
            (ja: "毎回ランチャーを表示",
             zh: "每次都显示启动器"),
        "Crash Report…":
            (ja: "クラッシュレポート…",
             zh: "崩溃报告…"),
        "Show Log":
            (ja: "ログを表示",
             zh: "显示日志"),
        "Stopping…":
            (ja: "停止中…",
             zh: "正在停止…"),
        "Game running":
            (ja: "ゲーム実行中",
             zh: "游戏运行中"),
        "Stop":
            (ja: "停止",
             zh: "停止"),
        "Close the running game. Use the game's Exit Game menu first to finish saving.":
            (ja: "実行中のゲームを終了します。先にゲーム内の「ゲーム終了」メニューでセーブを完了してください。",
             zh: "关闭正在运行的游戏。请先使用游戏内的“退出游戏”菜单完成存档。"),
        "Benchmark…":
            (ja: "ベンチマーク…",
             zh: "基准测试…"),
        "Measures how the game runs on this Mac: about 6 minutes, with its own save and settings":
            (ja: "この Mac でのゲームの動作を測定します。約 6 分、専用のセーブと設定を使用します",
             zh: "测量游戏在这台 Mac 上的运行表现：约 6 分钟，使用独立的存档和设置"),
        "Running…":
            (ja: "実行中…",
             zh: "运行中…"),
        "Play":
            (ja: "プレイ",
             zh: "开始游戏"),
        "Launcher":
            (ja: "ランチャー",
             zh: "启动器"),
        "Language":
            (ja: "言語",
             zh: "语言"),
        "System":
            (ja: "システム",
             zh: "跟随系统"),
        // General page
        "Display":
            (ja: "ディスプレイ",
             zh: "显示"),
        "Ultrawide or 16:10":
            (ja: "ウルトラワイド / 16:10",
             zh: "超宽屏或 16:10"),
        "Resolution":
            (ja: "解像度",
             zh: "分辨率"),
        "Native resolution. Uses your existing game UI; no UI files are installed.":
            (ja: "ネイティブ解像度。ゲーム本来の UI を使用し、UI ファイルは追加されません。",
             zh: "原生分辨率。使用游戏现有的界面，不会安装任何界面文件。"),
        "Output resolution":
            (ja: "出力解像度",
             zh: "输出分辨率"),
        "Window":
            (ja: "ウインドウ",
             zh: "窗口"),
        "Windowed":
            (ja: "ウインドウ",
             zh: "窗口化"),
        "Full screen":
            (ja: "フルスクリーン",
             zh: "全屏"),
        "Main display":
            (ja: "メインディスプレイ",
             zh: "主显示器"),
        "%@ (main)":
            (ja: "%@（メイン）",
             zh: "%@（主显示器）"),
        "Display not connected":
            (ja: "接続されていないディスプレイ",
             zh: "显示器未连接"),
        "The game opens on this display, in a window or full screen. If it isn't connected, the main display is used.":
            (ja: "ゲームはウインドウでもフルスクリーンでもこのディスプレイで開きます。接続されていない場合はメインディスプレイを使用します。",
             zh: "游戏会在此显示器上以窗口或全屏方式打开。如果它未连接，则使用主显示器。"),
        "Frame rate cap":
            (ja: "フレームレート上限",
             zh: "帧率上限"),
        "Above 60 FPS is experimental: the game was made for 30. 120 needs a 120 Hz display.":
            (ja: "60 FPS 超は実験的機能です（ゲームは 30 FPS 向けに作られています）。120 には 120 Hz のディスプレイが必要です。",
             zh: "高于 60 FPS 属于实验性功能：游戏原本按 30 FPS 设计。120 需要 120 Hz 显示器。"),
        "Game timing":
            (ja: "ゲームのタイミング",
             zh: "游戏计时"),
        "Fixed steps":
            (ja: "固定ステップ",
             zh: "固定步长"),
        "Variable":
            (ja: "可変",
             zh: "可变"),
        "Fixed steps: the game advances the same amount every frame, as on the PS4. If a scene can't hold the cap it slows down a little. Variable: keeps full speed when frames are late, but uneven frames can drop you through moving lifts and floors. Applies at the next start.":
            (ja: "固定ステップ：PS4 と同じく、毎フレーム同じ量だけゲームが進みます。上限を維持できない場面では少し遅くなります。可変：フレームが遅れても速度を保ちますが、フレームが不均一だと動くリフトや床をすり抜けて落ちることがあります。次回起動時に適用されます。",
             zh: "固定步长：与 PS4 上一样，游戏每帧推进相同的量。场景无法维持帧率上限时会略微变慢。可变：帧延迟时仍保持全速，但帧时间不均匀可能让你穿过移动的升降梯和地板掉下去。下次启动时生效。"),
        "%@ FPS always uses variable timing: there is no fixed-step patch for it. Uneven frames can drop you through moving lifts.":
            (ja: "%@ FPS では常に可変タイミングになります（固定ステップ用のパッチがありません）。フレームが不均一だと動くリフトをすり抜けて落ちることがあります。",
             zh: "%@ FPS 始终使用可变计时：没有对应的固定步长补丁。帧时间不均匀可能让你穿过移动的升降梯掉下去。"),
        "Upscaler":
            (ja: "アップスケーラー",
             zh: "超分辨率"),
        "Ultrawide runs at the selected native resolution, with upscaling off.":
            (ja: "ウルトラワイドは選択したネイティブ解像度で動作し、アップスケールはオフになります。",
             zh: "超宽屏以所选的原生分辨率运行，并关闭超分辨率。"),
        "Quality":
            (ja: "クオリティ",
             zh: "质量"),
        "Renders":
            (ja: "描画",
             zh: "渲染"),
        "Sharpening":
            (ja: "シャープ化",
             zh: "锐化"),
        "Strength":
            (ja: "強さ",
             zh: "强度"),
        "Effects":
            (ja: "エフェクト",
             zh: "画面效果"),
        "Show FPS":
            (ja: "FPS を表示",
             zh: "显示 FPS"),
        "Locations":
            (ja: "保存場所",
             zh: "位置"),
        "Game folder":
            (ja: "ゲームフォルダ",
             zh: "游戏文件夹"),
        "Save folder":
            (ja: "セーブフォルダ",
             zh: "存档文件夹"),
        "Not chosen":
            (ja: "未選択",
             zh: "未选择"),
        "Choose…":
            (ja: "選択…",
             zh: "选择…"),
        "Reset to Default":
            (ja: "デフォルトに戻す",
             zh: "恢复默认"),
        "Off":
            (ja: "オフ",
             zh: "关"),
        "On":
            (ja: "オン",
             zh: "开"),
        "Auto":
            (ja: "自動",
             zh: "自动"),
        "Native AA":
            (ja: "ネイティブ AA",
             zh: "原生抗锯齿"),
        "Balanced":
            (ja: "バランス",
             zh: "均衡"),
        "Performance":
            (ja: "パフォーマンス",
             zh: "性能"),
        "Ultra Performance":
            (ja: "ウルトラパフォーマンス",
             zh: "超级性能"),
        "Depth of field":
            (ja: "被写界深度",
             zh: "景深"),
        "Motion blur":
            (ja: "モーションブラー",
             zh: "动态模糊"),
        "Chromatic aberration":
            (ja: "色収差",
             zh: "色差"),
        "Ambient occlusion (SSAO)":
            (ja: "アンビエントオクルージョン（SSAO）",
             zh: "环境光遮蔽（SSAO）"),
        "Shadows from dynamic lights":
            (ja: "動的ライトの影",
             zh: "动态光源阴影"),
        "Screen-space reflections (not in the original)":
            (ja: "スクリーンスペース反射（原作にはありません）",
             zh: "屏幕空间反射（原版没有）"),
        "The game's own anti-aliasing":
            (ja: "ゲーム本来のアンチエイリアス",
             zh: "游戏自带的抗锯齿"),
        "Skip intro videos":
            (ja: "オープニング動画をスキップ",
             zh: "跳过开场视频"),
        "Free camera (Cross + L3)":
            (ja: "フリーカメラ（✕ + L3）",
             zh: "自由视角（✕ + L3）"),
        "Debug menu (needs font files)":
            (ja: "デバッグメニュー（フォントファイルが必要）",
             zh: "调试菜单（需要字体文件）"),
        "5120 × 2160 needs about 9 GB of graphics memory. On Macs with less than 32 GB of memory expect slowdowns, missing textures or crashes. 3440 × 1440 is the same shape and much lighter.":
            (ja: "5120 × 2160 には約 9 GB のグラフィックスメモリが必要です。メモリが 32 GB 未満の Mac では、速度低下、テクスチャの欠落、クラッシュが起こることがあります。3440 × 1440 は同じ比率でずっと軽くなります。",
             zh: "5120 × 2160 需要约 9 GB 的图形内存。在内存不足 32 GB 的 Mac 上可能出现卡顿、贴图缺失或崩溃。3440 × 1440 比例相同，负担小得多。"),
        "This resolution may exceed available graphics memory on Macs with less than 24 GB of memory, causing slowdowns or crashes. Try a smaller resolution of the same shape if this happens.":
            (ja: "メモリが 24 GB 未満の Mac では、この解像度は使用可能なグラフィックスメモリを超え、速度低下やクラッシュの原因になることがあります。その場合は同じ比率の小さい解像度をお試しください。",
             zh: "在内存不足 24 GB 的 Mac 上，此分辨率可能超出可用的图形内存，导致卡顿或崩溃。如果出现这种情况，请尝试相同比例的较低分辨率。"),
        "Choose the game folder (CUSA03173 with update 1.09, the folder containing eboot.bin).":
            (ja: "ゲームフォルダを選択してください（アップデート 1.09 適用済みの CUSA03173、eboot.bin のあるフォルダ）。",
             zh: "请选择游戏文件夹（已更新到 1.09 的 CUSA03173，即包含 eboot.bin 的文件夹）。"),
        "Choose your Bloodborne game folder (CUSA03173, the one containing eboot.bin).":
            (ja: "Bloodborne のゲームフォルダを選択してください（CUSA03173、eboot.bin のあるフォルダ）。",
             zh: "请选择 Bloodborne 的游戏文件夹（CUSA03173，即包含 eboot.bin 的文件夹）。"),
        "There is no eboot.bin in this folder. Choose the folder that contains eboot.bin, sce_sys and dvdroot_ps4.":
            (ja: "このフォルダに eboot.bin がありません。eboot.bin、sce_sys、dvdroot_ps4 のあるフォルダを選択してください。",
             zh: "此文件夹中没有 eboot.bin。请选择包含 eboot.bin、sce_sys 和 dvdroot_ps4 的文件夹。"),
        "The game check failed (status %@).":
            (ja: "ゲームのチェックに失敗しました（ステータス %@）。",
             zh: "游戏校验失败（状态 %@）。"),
        "Choose the folder for your saves.":
            (ja: "セーブデータ用のフォルダを選択してください。",
             zh: "请选择用于保存存档的文件夹。"),
        "The save folder %@ is missing (is its drive connected?).":
            (ja: "セーブフォルダ %@ が見つかりません（ドライブは接続されていますか？）。",
             zh: "找不到存档文件夹 %@（它所在的磁盘已连接吗？）。"),
        "Cannot create %@: %@":
            (ja: "%@ を作成できません：%@",
             zh: "无法创建 %@：%@"),
        "%@ is a file, not a folder.":
            (ja: "%@ はフォルダではなくファイルです。",
             zh: "%@ 是文件，不是文件夹。"),
        "%@ is not writable.":
            (ja: "%@ に書き込めません。",
             zh: "%@ 不可写入。"),
        "Copy your saves to the new folder?":
            (ja: "セーブデータを新しいフォルダにコピーしますか？",
             zh: "要把存档复制到新文件夹吗？"),
        "Your saves in %@ are copied to %@. The originals stay where they are.":
            (ja: "%@ のセーブデータを %@ にコピーします。元のデータはそのまま残ります。",
             zh: "%@ 中的存档将复制到 %@。原存档保留在原处。"),
        "Copy Saves":
            (ja: "セーブをコピー",
             zh: "复制存档"),
        "Don't Copy":
            (ja: "コピーしない",
             zh: "不复制"),
        "Copying the saves failed: %@":
            (ja: "セーブデータのコピーに失敗しました：%@",
             zh: "复制存档失败：%@"),
        "Could not save the settings: %@":
            (ja: "設定を保存できませんでした：%@",
             zh: "无法保存设置：%@"),
        // Advanced page
        "Detail":
            (ja: "ディテール",
             zh: "细节"),
        "Model detail":
            (ja: "モデルのディテール",
             zh: "模型细节"),
        "Highest":
            (ja: "最高",
             zh: "最高"),
        "Game default":
            (ja: "ゲームのデフォルト",
             zh: "游戏默认"),
        "Lower":
            (ja: "低め",
             zh: "较低"),
        "Lowest":
            (ja: "最低",
             zh: "最低"),
        "Distance at which characters and objects switch to simpler models.":
            (ja: "キャラクターやオブジェクトが簡易モデルに切り替わる距離です。",
             zh: "角色和物体切换为简化模型的距离。"),
        "Live resolution changes":
            (ja: "解像度のライブ変更",
             zh: "实时切换分辨率"),
        "On: output resolution and quality change in the game without a restart, but post-processing stays at 1080p (slower). Off: everything renders at the chosen size. Scene resolutions above 1080p always use it.":
            (ja: "オン：出力解像度とクオリティを再起動なしでゲーム中に変更できますが、ポストプロセスは 1080p のままです（低速）。オフ：すべて選択したサイズで描画します。1080p を超えるシーン解像度では常に使用されます。",
             zh: "开：无需重启即可在游戏中更改输出分辨率和质量，但后期处理保持 1080p（较慢）。关：全部按所选尺寸渲染。场景分辨率高于 1080p 时始终使用此功能。"),
        "Avoid white scenes (MetalFX)":
            (ja: "白い画面を回避（MetalFX）",
             zh: "避免场景发白（MetalFX）"),
        "For MetalFX output above 1080p. Keeps post-processing at 1080p to avoid white scenes, but can reduce performance. Requires a restart.":
            (ja: "1080p を超える MetalFX 出力用です。ポストプロセスを 1080p に保って白い画面を防ぎますが、パフォーマンスが下がることがあります。再起動が必要です。",
             zh: "用于高于 1080p 的 MetalFX 输出。将后期处理保持在 1080p 以避免场景发白，但可能降低性能。需要重启。"),
        "Shader compilation":
            (ja: "シェーダーのコンパイル",
             zh: "着色器编译"),
        "New shaders":
            (ja: "新しいシェーダー",
             zh: "新着色器"),
        "Wait (no pop-in, stutters)":
            (ja: "待つ（ポップインなし、カクつきあり）",
             zh: "等待（无突然出现，会卡顿）"),
        "Skip (never stutters, pop-in)":
            (ja: "スキップ（カクつきなし、ポップインあり）",
             zh: "跳过（不卡顿，会突然出现）"),
        "Shaders the game hasn't used on this Mac before compile in the background. Shaders seen before load at start either way.":
            (ja: "この Mac でゲームがまだ使っていないシェーダーはバックグラウンドでコンパイルされます。使用済みのシェーダーはどちらの設定でも起動時に読み込まれます。",
             zh: "游戏在这台 Mac 上尚未用过的着色器会在后台编译。用过的着色器无论如何都会在启动时加载。"),
        "Texture compression":
            (ja: "テクスチャ圧縮",
             zh: "纹理压缩"),
        "Lets the GPU compress the game's textures and render targets without any loss of quality. They take less memory bandwidth, which is the main limit on base and Pro chips: up to 40% more FPS there, less on Max and Ultra chips. Turn it off if you see wrong colours or broken textures. Applies at the next start.":
            (ja: "GPU がゲームのテクスチャとレンダーターゲットを画質を落とさずに圧縮します。無印や Pro チップで主な制約となるメモリ帯域の消費が減り、最大 40% FPS が向上します（Max や Ultra チップでは効果が小さくなります）。色がおかしい、テクスチャが壊れる場合はオフにしてください。次回起動時に適用されます。",
             zh: "让 GPU 无损压缩游戏的纹理和渲染目标。这样占用的内存带宽更少，而内存带宽正是基础款和 Pro 芯片的主要瓶颈：在这些芯片上 FPS 最多可提升 40%，在 Max 和 Ultra 芯片上提升较小。如果出现颜色错误或纹理损坏，请关闭它。下次启动时生效。"),
        "Startup and debugging":
            (ja: "起動とデバッグ",
             zh: "启动与调试"),
        "Metal Performance HUD":
            (ja: "Metal パフォーマンス HUD",
             zh: "Metal 性能 HUD"),
        "Apple's Metal HUD: FPS, GPU time and frame pacing, top right":
            (ja: "Apple の Metal HUD：FPS、GPU 時間、フレームペーシングを右上に表示します",
             zh: "Apple 的 Metal HUD：在右上角显示 FPS、GPU 时间和帧节奏"),
        "Free camera: hold Cross and press L3 (keyboard: F + Left Ctrl). Debug menu: left touchpad / G; needs DbgFont14h.ccm and DbgFont14h.tpf in dvdroot_ps4/font from Nexus mod #253.":
            (ja: "フリーカメラ：✕ を押しながら L3（キーボード：F + 左 Ctrl）。デバッグメニュー：タッチパッド左 / G。Nexus の Mod #253 の DbgFont14h.ccm と DbgFont14h.tpf が dvdroot_ps4/font に必要です。",
             zh: "自由视角：按住 ✕ 再按 L3（键盘：F + 左 Ctrl）。调试菜单：触摸板左侧 / G；需要把 Nexus 模组 #253 中的 DbgFont14h.ccm 和 DbgFont14h.tpf 放入 dvdroot_ps4/font。"),
        "Choose an upscaler on the General tab to adjust these options.":
            (ja: "これらのオプションを調整するには、「一般」タブでアップスケーラーを選択してください。",
             zh: "请先在“常规”页选择超分辨率方式，再调整这些选项。"),
        "Subpixel jitter":
            (ja: "サブピクセルジッター",
             zh: "子像素抖动"),
        "Offsets each frame by a fraction of a pixel so the upscaler gathers more detail over time.":
            (ja: "各フレームを 1 ピクセル未満だけずらし、アップスケーラーが時間をかけてより多くのディテールを集められるようにします。",
             zh: "让每一帧偏移不到一个像素，使超分辨率能随时间收集更多细节。"),
        "Character motion vectors":
            (ja: "キャラクターのモーションベクター",
             zh: "角色运动矢量"),
        "Precise vectors for animated objects: clothing and weapons break up less in motion.":
            (ja: "アニメーションするオブジェクトの正確なベクターです。動いている衣服や武器の乱れが減ります。",
             zh: "为带动画的物体提供精确的矢量：运动中的衣物和武器更不容易出现破碎。"),
        "Reactive mask":
            (ja: "リアクティブマスク",
             zh: "反应遮罩"),
        "TAA doesn't use a reactive mask. Choose FSR or MetalFX to adjust it.":
            (ja: "TAA はリアクティブマスクを使いません。調整するには FSR または MetalFX を選択してください。",
             zh: "TAA 不使用反应遮罩。请选择 FSR 或 MetalFX 后再调整。"),
        "Choose FSR or MetalFX on the General tab to adjust these options.":
            (ja: "これらのオプションを調整するには、「一般」タブで FSR または MetalFX を選択してください。",
             zh: "请先在“常规”页选择 FSR 或 MetalFX，再调整这些选项。"),
        "Enable mask":
            (ja: "マスクを有効にする",
             zh: "启用遮罩"),
        "Marks transparent effects (particles, haze) so the upscaler relies less on previous frames. Less trailing behind effects, but shimmering returns beneath them.":
            (ja: "半透明エフェクト（パーティクル、もや）に印を付け、アップスケーラーが過去のフレームに頼りすぎないようにします。エフェクトの残像は減りますが、その下でちらつきが戻ります。",
             zh: "标记透明特效（粒子、雾气），让超分辨率减少对前几帧的依赖。特效后的拖影会减少，但其下方的闪烁会重新出现。"),
        "Scale":
            (ja: "スケール",
             zh: "缩放"),
        "Threshold":
            (ja: "しきい値",
             zh: "阈值"),
        "Maximum":
            (ja: "最大値",
             zh: "最大值"),
        // Controls page
        "Mouse":
            (ja: "マウス",
             zh: "鼠标"),
        "Mouse camera":
            (ja: "マウスでカメラ操作",
             zh: "鼠标控制视角"),
        "Click in the game window to turn the camera with the mouse; ⌘ gives the cursor back. Controllers always work.":
            (ja: "ゲームウインドウをクリックするとマウスでカメラを動かせます。⌘ でカーソルが戻ります。コントローラーはいつでも使えます。",
             zh: "点击游戏窗口后即可用鼠标转动视角；按 ⌘ 取回光标。手柄始终可用。"),
        "Sensitivity":
            (ja: "感度",
             zh: "灵敏度"),
        "How far the camera turns for a mouse movement.":
            (ja: "マウスの動きに対してカメラがどれだけ回るかです。",
             zh: "鼠标移动时视角转动的幅度。"),
        "Vertical speed":
            (ja: "上下の速さ",
             zh: "垂直速度"),
        "How fast the camera turns up and down, relative to left and right.":
            (ja: "左右に対する、カメラの上下回転の速さです。",
             zh: "视角上下转动相对于左右转动的速度。"),
        "Invert vertical":
            (ja: "上下を反転",
             zh: "垂直反转"),
        "Turning still too slow? Raise Camera speed in the game's own options too. Mouse buttons and the wheel reach the game while it holds the cursor. The in-game menu (⌘,) has these sliders too, live.":
            (ja: "まだ回転が遅い場合は、ゲーム内オプションのカメラ速度も上げてください。マウスボタンとホイールは、ゲームがカーソルを捕まえている間だけゲームに届きます。ゲーム内メニュー（⌘,）にも同じスライダーがあり、その場で反映されます。",
             zh: "转动还是太慢？请同时调高游戏自身选项中的镜头速度。鼠标按键和滚轮只有在游戏捕获光标时才会传给游戏。游戏内菜单（⌘,）里也有这些滑块，可实时调整。"),
        "Keyboard":
            (ja: "キーボード",
             zh: "键盘"),
        "Camera keys are the d-pad with the mouse camera":
            (ja: "マウスカメラ使用時はカメラキーを方向キーにする",
             zh: "使用鼠标视角时，视角键充当方向键"),
        "While the mouse turns the camera, the camera keys (I, J, K and L by default) scroll menus and switch weapons and items instead.":
            (ja: "マウスでカメラを動かしている間、カメラキー（デフォルトは I、J、K、L）はメニューのスクロールや武器・アイテムの切り替えに使われます。",
             zh: "用鼠标转动视角时，视角键（默认为 I、J、K、L）改为滚动菜单以及切换武器和道具。"),
        "Controller":
            (ja: "コントローラー",
             zh: "手柄"),
        "L3 + R3 opens the in-game menu":
            (ja: "L3 + R3 でゲーム内メニューを開く",
             zh: "L3 + R3 打开游戏内菜单"),
        "Pressing both sticks in opens the settings menu during play. Turn it off if you press them by accident; ⌘, still opens it.":
            (ja: "プレイ中に両方のスティックを押し込むと設定メニューが開きます。誤って押してしまう場合はオフにしてください。⌘, では引き続き開けます。",
             zh: "游戏中同时按下两个摇杆会打开设置菜单。如果经常误触可以关闭；⌘, 仍可打开菜单。"),
        "Buttons as on a PlayStation pad":
            (ja: "ボタンは PlayStation コントローラーと同じ",
             zh: "按键与 PlayStation 手柄一致"),
        "Buttons remapped":
            (ja: "ボタンを変更済み",
             zh: "按键已重新映射"),
        "Controller Buttons…":
            (ja: "コントローラーのボタン…",
             zh: "手柄按键…"),
        "Controller Buttons":
            (ja: "コントローラーのボタン",
             zh: "手柄按键"),
        "Choose what each controller button does in the game.":
            (ja: "コントローラーの各ボタンのゲーム内での役割を選びます。",
             zh: "选择手柄每个按键在游戏中的功能。"),
        "A key can do several things; ⌘ is kept for Mac shortcuts.":
            (ja: "1 つのキーに複数の操作を割り当てられます。⌘ は Mac のショートカット用です。",
             zh: "一个按键可以绑定多个功能；⌘ 保留给 Mac 快捷键。"),
        "Reset to Defaults":
            (ja: "デフォルトに戻す",
             zh: "恢复默认"),
        "Also used by: ":
            (ja: "次でも使用中：",
             zh: "同时用于："),
        "%@ – click to change":
            (ja: "%@ – クリックして変更",
             zh: "%@ – 点击更改"),
        "Click, then press a key or mouse button":
            (ja: "クリックしてから、キーまたはマウスボタンを押してください",
             zh: "点击后按下按键或鼠标按键"),
        "%@: click a slot, then press the button you want for it.":
            (ja: "%@：スロットをクリックしてから、割り当てたいボタンを押してください。",
             zh: "%@：点击一个槽位，然后按下要绑定的按键。"),
        "Connect a controller, then click a slot and press the button you want for it.":
            (ja: "コントローラーを接続し、スロットをクリックしてから、割り当てたいボタンを押してください。",
             zh: "请连接手柄，然后点击一个槽位并按下要绑定的按键。"),
        "Press…":
            (ja: "押してください…",
             zh: "请按键…"),
        "%@ – click, then press a controller button":
            (ja: "%@ – クリックしてから、コントローラーのボタンを押してください",
             zh: "%@ – 点击后按下手柄按键"),
        "Click, then press a controller button":
            (ja: "クリックしてから、コントローラーのボタンを押してください",
             zh: "点击后按下手柄按键"),
        "Clear Slot":
            (ja: "スロットを消去",
             zh: "清除槽位"),
        "Done":
            (ja: "完了",
             zh: "完成"),
        "Press a key, click a mouse button or turn the wheel here":
            (ja: "ここでキーを押すか、マウスボタンをクリックするか、ホイールを回してください",
             zh: "请在此处按下按键、点击鼠标按键或滚动滚轮"),
        "%@: let go for this key alone, or press another key or mouse button with it":
            (ja: "%@：このキーだけなら離してください。組み合わせるなら、押したまま別のキーかマウスボタンを押してください",
             zh: "%@：松开即只绑定此键；或按住它再按另一个按键或鼠标按键，绑定为组合键"),
        "Now: %@":
            (ja: "現在：%@",
             zh: "当前：%@"),
        "Clear":
            (ja: "消去",
             zh: "清除"),
        "Cancel":
            (ja: "キャンセル",
             zh: "取消"),
        "The game can't read that key.":
            (ja: "そのキーはゲームで読み取れません。",
             zh: "游戏无法识别该按键。"),
        "⌘ is kept for Mac shortcuts (and gives the cursor back).":
            (ja: "⌘ は Mac のショートカット用です（カーソルを戻す操作にも使います）。",
             zh: "⌘ 保留给 Mac 快捷键（并用于取回光标）。"),
        "Movement":
            (ja: "移動",
             zh: "移动"),
        "Camera":
            (ja: "カメラ",
             zh: "视角"),
        "Combat":
            (ja: "戦闘",
             zh: "战斗"),
        "Actions":
            (ja: "アクション",
             zh: "动作"),
        "D-pad":
            (ja: "方向キー",
             zh: "方向键"),
        "Menus":
            (ja: "メニュー",
             zh: "菜单"),
        "Forward":
            (ja: "前",
             zh: "前进"),
        "Back":
            (ja: "後ろ",
             zh: "后退"),
        "Left":
            (ja: "左",
             zh: "左"),
        "Right":
            (ja: "右",
             zh: "右"),
        "Up":
            (ja: "上",
             zh: "上"),
        "Down":
            (ja: "下",
             zh: "下"),
        "Dodge (tap Circle)":
            (ja: "回避（○ を短く押す）",
             zh: "闪避（轻按 ○）"),
        "Run (hold Circle)":
            (ja: "ダッシュ（○ を長押し）",
             zh: "奔跑（按住 ○）"),
        "Dodge, sprint (Circle)":
            (ja: "回避・ダッシュ（○）",
             zh: "闪避 / 奔跑（○）"),
        "Walk (hold)":
            (ja: "歩く（長押し）",
             zh: "步行（按住）"),
        "Attack (R1)":
            (ja: "攻撃（R1）",
             zh: "攻击（R1）"),
        "Charged attack (R2)":
            (ja: "強攻撃・溜め攻撃（R2）",
             zh: "重击 / 蓄力攻击（R2）"),
        "Transform weapon (L1)":
            (ja: "武器変形（L1）",
             zh: "武器变形（L1）"),
        "Firearm (L2)":
            (ja: "銃（L2）",
             zh: "枪械（L2）"),
        "Lock on (R3)":
            (ja: "ロックオン（R3）",
             zh: "锁定（R3）"),
        "Interact, confirm (Cross)":
            (ja: "調べる・決定（✕）",
             zh: "互动 / 确认（✕）"),
        "Back (Circle)":
            (ja: "戻る（○）",
             zh: "返回（○）"),
        "Quick item (Square)":
            (ja: "クイックアイテム（□）",
             zh: "快捷道具（□）"),
        "Blood Vial (Triangle)":
            (ja: "輸血液（△）",
             zh: "采血瓶（△）"),
        "Left (left-hand weapon)":
            (ja: "左（左手武器）",
             zh: "左（左手武器）"),
        "Right (right-hand weapon)":
            (ja: "右（右手武器）",
             zh: "右（右手武器）"),
        "Menu (Options)":
            (ja: "メニュー（OPTIONS）",
             zh: "菜单（OPTIONS）"),
        "Gestures (touchpad left)":
            (ja: "ジェスチャー（タッチパッド左）",
             zh: "手势（触摸板左侧）"),
        "Touchpad right side":
            (ja: "タッチパッド右",
             zh: "触摸板右侧"),
        "D-pad up":
            (ja: "方向キー上",
             zh: "方向键上"),
        "D-pad down":
            (ja: "方向キー下",
             zh: "方向键下"),
        "D-pad left (left-hand weapon)":
            (ja: "方向キー左（左手武器）",
             zh: "方向键左（左手武器）"),
        "D-pad right (right-hand weapon)":
            (ja: "方向キー右（右手武器）",
             zh: "方向键右（右手武器）"),
        "Gestures (touchpad)":
            (ja: "ジェスチャー（タッチパッド）",
             zh: "手势（触摸板）"),
        "Select (Share, Create, View)":
            (ja: "Select（Share、Create、View）",
             zh: "Select（Share、Create、View）"),
        "Misc (mic, capture, Xbox Share)":
            (ja: "Misc（マイク、キャプチャ、Xbox Share）",
             zh: "Misc（麦克风、截图、Xbox Share）"),
        "Left click":
            (ja: "左クリック",
             zh: "左键"),
        "Right click":
            (ja: "右クリック",
             zh: "右键"),
        "Middle click":
            (ja: "中クリック",
             zh: "中键"),
        "Back button":
            (ja: "戻るボタン",
             zh: "后退侧键"),
        "Forward button":
            (ja: "進むボタン",
             zh: "前进侧键"),
        "Wheel up":
            (ja: "ホイール上",
             zh: "滚轮上"),
        "Wheel down":
            (ja: "ホイール下",
             zh: "滚轮下"),
        "⇧ Left":
            (ja: "左 ⇧",
             zh: "左 ⇧"),
        "⇧ Right":
            (ja: "右 ⇧",
             zh: "右 ⇧"),
        "⌥ Left":
            (ja: "左 ⌥",
             zh: "左 ⌥"),
        "⌥ Right":
            (ja: "右 ⌥",
             zh: "右 ⌥"),
        "⌃ Left":
            (ja: "左 ⌃",
             zh: "左 ⌃"),
        "⌃ Right":
            (ja: "右 ⌃",
             zh: "右 ⌃"),
        // Starting the game
        "Bloodborne is already running. Only one copy of the game can run at a time.":
            (ja: "Bloodborne はすでに実行中です。同時に実行できるのは 1 つだけです。",
             zh: "Bloodborne 已在运行。同一时间只能运行一个游戏实例。"),
        "Bloodborne needs Rosetta 2":
            (ja: "Bloodborne には Rosetta 2 が必要です",
             zh: "Bloodborne 需要 Rosetta 2"),
        "The game runs its original PlayStation 4 code through Rosetta 2, Apple's translator for Intel code. Install it now? (macOS asks for an administrator password.)":
            (ja: "このゲームは、Apple の Intel コード変換機能である Rosetta 2 を通して、PlayStation 4 版のオリジナルコードを実行します。今すぐインストールしますか？（macOS が管理者パスワードを求めます。）",
             zh: "游戏通过 Rosetta 2（Apple 的 Intel 代码转译器）运行原版 PlayStation 4 代码。现在安装吗？（macOS 会要求输入管理员密码。）"),
        "Install":
            (ja: "インストール",
             zh: "安装"),
        "Rosetta 2 was not installed. In Terminal, run: softwareupdate --install-rosetta":
            (ja: "Rosetta 2 をインストールできませんでした。ターミナルで次を実行してください：softwareupdate --install-rosetta",
             zh: "Rosetta 2 未能安装。请在终端中运行：softwareupdate --install-rosetta"),
        "The save folder is not available":
            (ja: "セーブフォルダを使用できません",
             zh: "存档文件夹不可用"),
        "Use Default":
            (ja: "デフォルトを使用",
             zh: "使用默认位置"),
        "The game could not start: %@":
            (ja: "ゲームを起動できませんでした：%@",
             zh: "游戏无法启动：%@"),
        "Your save could not be backed up":
            (ja: "セーブデータをバックアップできませんでした",
             zh: "无法备份存档"),
        "The launcher backs up your save before the first launch with a new set of mods or cheats, and copying %@ to %@ failed: %@":
            (ja: "ランチャーは、Mod やチートの組み合わせを変えて最初に起動する前にセーブデータをバックアップしますが、%@ から %@ へのコピーに失敗しました：%@",
             zh: "启动器会在首次使用一组新的模组或作弊启动前备份存档，但将 %@ 复制到 %@ 失败：%@"),
        "Play Anyway":
            (ja: "このままプレイ",
             zh: "仍然开始游戏"),
        "This copy of Bloodborne.app is incomplete":
            (ja: "この Bloodborne.app は不完全です",
             zh: "这份 Bloodborne.app 不完整"),
        "Download the app again, or run scripts/macos/make_app.sh in the repository.":
            (ja: "アプリをダウンロードし直すか、リポジトリで scripts/macos/make_app.sh を実行してください。",
             zh: "请重新下载应用，或在仓库中运行 scripts/macos/make_app.sh。"),
        "About Bloodborne":
            (ja: "Bloodborne について",
             zh: "关于 Bloodborne"),
        "Check for Updates…":
            (ja: "アップデートを確認…",
             zh: "检查更新…"),
        "Hide Bloodborne":
            (ja: "Bloodborne を非表示",
             zh: "隐藏 Bloodborne"),
        "Quit Bloodborne":
            (ja: "Bloodborne を終了",
             zh: "退出 Bloodborne"),
        // Crash report
        "Bloodborne stopped":
            (ja: "Bloodborne が停止しました",
             zh: "Bloodborne 已停止运行"),
        "Please report it so it can be fixed: Report on GitHub opens a new issue with this report filled in, and shows a diagnostics zip (the full log and macOS's crash reports) to drag onto it. Nothing is sent until you submit the issue. Your home folder is shown as ~; other paths (a drive under /Volumes) appear as they are.":
            (ja: "修正できるよう、ぜひ報告してください。「GitHub で報告」を押すと、このレポートが入力された新しい Issue が開き、そこにドラッグする診断 zip（完全なログと macOS のクラッシュレポート）が表示されます。Issue を送信するまで何も送られません。ホームフォルダは ~ と表示され、その他のパス（/Volumes 以下のドライブなど）はそのまま表示されます。",
             zh: "请报告此问题以便修复：“在 GitHub 上报告”会打开一个已填好本报告的新 issue，并显示一个诊断 zip（完整日志和 macOS 的崩溃报告），请把它拖到 issue 上。在你提交 issue 之前不会发送任何内容。你的个人文件夹显示为 ~；其他路径（/Volumes 下的磁盘）按原样显示。"),
        "Collecting macOS's crash reports and the log…":
            (ja: "macOS のクラッシュレポートとログを収集中…",
             zh: "正在收集 macOS 的崩溃报告和日志…"),
        "Diagnostics: %@":
            (ja: "診断データ：%@",
             zh: "诊断文件：%@"),
        "Show in Finder":
            (ja: "Finder に表示",
             zh: "在访达中显示"),
        "Show Log in Finder":
            (ja: "ログを Finder に表示",
             zh: "在访达中显示日志"),
        "Copied":
            (ja: "コピーしました",
             zh: "已复制"),
        "Copy Report":
            (ja: "レポートをコピー",
             zh: "复制报告"),
        "Report on GitHub":
            (ja: "GitHub で報告",
             zh: "在 GitHub 上报告"),
        "Close":
            (ja: "閉じる",
             zh: "关闭"),
        // Benchmark
        "The benchmark is not available":
            (ja: "ベンチマークは利用できません",
             zh: "基准测试不可用"),
        "The benchmark needs its save and recorded route (route.pad and savedata/) in %@. This build doesn't include them.":
            (ja: "ベンチマークには、専用のセーブと記録済みのルート（route.pad と savedata/）が %@ に必要です。このビルドには含まれていません。",
             zh: "基准测试需要在 %@ 中放置专用存档和录制的路线（route.pad 和 savedata/）。此版本未包含它们。"),
        "Bloodborne is already running. Quit the game before starting the benchmark.":
            (ja: "Bloodborne はすでに実行中です。ベンチマークを始める前にゲームを終了してください。",
             zh: "Bloodborne 已在运行。请先退出游戏再开始基准测试。"),
        "The game needs Rosetta 2: press Play once to install it.":
            (ja: "ゲームには Rosetta 2 が必要です。一度「プレイ」を押してインストールしてください。",
             zh: "游戏需要 Rosetta 2：请先点一次“开始游戏”来安装它。"),
        "Run the benchmark?":
            (ja: "ベンチマークを実行しますか？",
             zh: "要运行基准测试吗？"),
        "The game starts twice, about 6 minutes in all: a recorded walk through Central Yharnam, then 30 seconds standing still without a frame cap. Don't touch the game window, the keyboard or a controller while it runs: the walk is replayed, and any input would change it.\n\nThe benchmark uses its own save and fixed settings (1920 × 1080, upscaler off, windowed). Your saves and settings are not changed. The results open here when it's done.":
            (ja: "ゲームを 2 回起動し、合計で約 6 分かかります。ヤーナム市街を歩く記録済みルートの再生と、フレームレート上限なしで 30 秒間立ち止まる測定です。実行中はゲームウインドウ、キーボード、コントローラーに触れないでください。記録した操作を再生しているため、入力があると結果が変わります。\n\nベンチマークは専用のセーブと固定の設定（1920 × 1080、アップスケーラーオフ、ウインドウ表示）を使用します。あなたのセーブと設定は変更されません。完了すると結果がここに表示されます。",
             zh: "游戏会启动两次，总共约 6 分钟：先回放一段在亚楠中心区行走的录制路线，然后在不限帧率的情况下静止站立 30 秒。运行期间请勿触碰游戏窗口、键盘或手柄：行走是回放的，任何输入都会改变结果。\n\n基准测试使用独立的存档和固定设置（1920 × 1080、关闭超分辨率、窗口化）。你的存档和设置不会被更改。完成后结果会显示在这里。"),
        "Start Benchmark":
            (ja: "ベンチマークを開始",
             zh: "开始基准测试"),
        "The benchmark could not start: %@":
            (ja: "ベンチマークを開始できませんでした：%@",
             zh: "基准测试无法启动：%@"),
        "Benchmark: pass %@ of %@, %@…":
            (ja: "ベンチマーク：%2$@ 回中 %1$@ 回目、%3$@…",
             zh: "基准测试：第 %@ 轮，共 %@ 轮，%@…"),
        "route":
            (ja: "ルート",
             zh: "路线"),
        "uncapped":
            (ja: "上限なし",
             zh: "不限帧率"),
        "Benchmark results":
            (ja: "ベンチマーク結果",
             zh: "基准测试结果"),
        "Benchmark stopped":
            (ja: "ベンチマークが中断されました",
             zh: "基准测试已中止"),
        "A pass did not finish; the report shows what was measured. The game's logs are in the results folder.":
            (ja: "完了しなかった測定があります。レポートには測定できた分が表示されます。ゲームのログは結果フォルダにあります。",
             zh: "有一轮测试未完成；报告显示了已测得的数据。游戏日志在结果文件夹中。"),
        // Updates
        "Check for updates automatically?":
            (ja: "アップデートを自動的に確認しますか？",
             zh: "要自动检查更新吗？"),
        "Bloodborne can look for a new version of this port on its GitHub releases page once a day, when the launcher opens. Nothing is downloaded or installed without asking you. You can change this in the update window (Bloodborne → Check for Updates…), which works either way.":
            (ja: "Bloodborne は、ランチャーを開いたときに 1 日 1 回、GitHub のリリースページでこの移植版の新しいバージョンを確認できます。確認なしにダウンロードやインストールが行われることはありません。この設定はアップデートウインドウ（Bloodborne → アップデートを確認…）で変更でき、手動の確認はどちらの設定でも使えます。",
             zh: "Bloodborne 可以在启动器打开时每天一次到 GitHub 发布页查找此移植版的新版本。未经你同意不会下载或安装任何内容。你可以在更新窗口（Bloodborne → 检查更新…）中更改此设置，手动检查在两种设置下都可用。"),
        "Check Automatically":
            (ja: "自動的に確認",
             zh: "自动检查"),
        "Don't Check":
            (ja: "確認しない",
             zh: "不检查"),
        "Version %@ is available":
            (ja: "バージョン %@ が利用可能です",
             zh: "有新版本 %@"),
        "Version %@":
            (ja: "バージョン %@",
             zh: "版本 %@"),
        "Check for updates automatically":
            (ja: "アップデートを自動的に確認",
             zh: "自动检查更新"),
        "Include test builds":
            (ja: "テストビルドを含める",
             zh: "包含测试版"),
        "Also offer pre-releases and test builds: early fixes that may not be finished.":
            (ja: "プレリリースやテストビルドも通知します。未完成の可能性がある先行修正です。",
             zh: "同时提供预发布版和测试版：其中的早期修复可能尚未完成。"),
        "Checking for updates…":
            (ja: "アップデートを確認中…",
             zh: "正在检查更新…"),
        "You're up to date":
            (ja: "最新の状態です",
             zh: "已是最新版本"),
        "Bloodborne %@ is the newest version available.":
            (ja: "Bloodborne %@ は現在の最新バージョンです。",
             zh: "Bloodborne %@ 是目前的最新版本。"),
        "Update":
            (ja: "アップデート",
             zh: "更新"),
        "A new version of Bloodborne is available":
            (ja: "Bloodborne の新しいバージョンがあります",
             zh: "Bloodborne 有新版本可用"),
        "Version %@ is available; you have %@.":
            (ja: "バージョン %@ が利用可能です。現在のバージョンは %@ です。",
             zh: "有新版本 %@；你当前的版本是 %@。"),
        "Downloading…":
            (ja: "ダウンロード中…",
             zh: "正在下载…"),
        "Installing…":
            (ja: "インストール中…",
             zh: "正在安装…"),
        "Quit the game to install the update.":
            (ja: "アップデートをインストールするにはゲームを終了してください。",
             zh: "请退出游戏以安装更新。"),
        "The app is replaced and opens again; your saves and settings stay where they are.":
            (ja: "アプリが置き換えられ、もう一度開きます。セーブと設定はそのまま残ります。",
             zh: "应用会被替换并重新打开；你的存档和设置保持不变。"),
        "Skip This Version":
            (ja: "このバージョンをスキップ",
             zh: "跳过此版本"),
        "Release Page":
            (ja: "リリースページ",
             zh: "发布页面"),
        "Later":
            (ja: "あとで",
             zh: "稍后"),
        "Install Update":
            (ja: "アップデートをインストール",
             zh: "安装更新"),
        "OK":
            (ja: "OK",
             zh: "好"),
        // Mods & Cheats window
        "Mods":
            (ja: "Mod",
             zh: "模组"),
        "Cheats & Tweaks":
            (ja: "チートと調整",
             zh: "作弊与调整"),
        "Patch Files":
            (ja: "パッチファイル",
             zh: "补丁文件"),
        "Mods are merged when the game starts, without changing the game files. Mods lower in the list load later and win when two replace the same file. Drag to reorder.":
            (ja: "Mod はゲームファイルを変更せず、ゲーム起動時に統合されます。リストの下にある Mod ほど後から読み込まれ、同じファイルを置き換える場合は後のものが優先されます。ドラッグで並べ替えできます。",
             zh: "模组在游戏启动时合并，不会修改游戏文件。列表中靠下的模组后加载，两个模组替换同一文件时以后加载者为准。拖动可调整顺序。"),
        "No mods yet. Add a mod's folder or .zip, or put mod folders in the mods folder.":
            (ja: "Mod はまだありません。Mod のフォルダか .zip を追加するか、mods フォルダに Mod のフォルダを入れてください。",
             zh: "还没有模组。请添加模组的文件夹或 .zip，或把模组文件夹放进 mods 文件夹。"),
        "Add Mod…":
            (ja: "Mod を追加…",
             zh: "添加模组…"),
        "Remove…":
            (ja: "削除…",
             zh: "移除…"),
        "Load earlier":
            (ja: "先に読み込む",
             zh: "更早加载"),
        "Load later (wins conflicts)":
            (ja: "後で読み込む（競合時に優先）",
             zh: "更晚加载（冲突时优先）"),
        "Open Mods Folder":
            (ja: "Mod フォルダを開く",
             zh: "打开模组文件夹"),
        "Conflicts":
            (ja: "競合",
             zh: "冲突"),
        "The game is running: these apply at the next start, change them once it has ended.":
            (ja: "ゲームが実行中です。これらは次回起動時に適用されます。ゲーム終了後に変更してください。",
             zh: "游戏正在运行：这些设置在下次启动时生效，请在游戏结束后再更改。"),
        "Cheats change the game's code. Your save is backed up before the first launch with a new set of mods or cheats.":
            (ja: "チートはゲームのコードを変更します。Mod やチートの組み合わせを変えて最初に起動する前に、セーブデータがバックアップされます。",
             zh: "作弊会修改游戏代码。首次使用一组新的模组或作弊启动前，会先备份你的存档。"),
        "Applies at the next start. Options that can't be combined turn each other off.":
            (ja: "次回起動時に適用されます。併用できないオプションは互いにオフになります。",
             zh: "下次启动时生效。无法同时使用的选项会互相关闭。"),
        "Patches from other sources' patch files, applied at the next start. Only patches for app version 01.09 (the version this port runs) are listed.":
            (ja: "他の配布元のパッチファイルに含まれるパッチです。次回起動時に適用されます。アプリバージョン 01.09（この移植版が動かすバージョン）用のパッチだけが表示されます。",
             zh: "来自其他来源补丁文件的补丁，下次启动时应用。只列出适用于应用版本 01.09（本移植版运行的版本）的补丁。"),
        "No patch files":
            (ja: "パッチファイルがありません",
             zh: "没有补丁文件"),
        "Put shadPS4 or GoldHEN patch files (.xml) for Bloodborne, app version 01.09, in the patches folder: their patches show up here to turn on or off. The port's own patches (graphics, frame rate and the cheats) are built in.":
            (ja: "Bloodborne（アプリバージョン 01.09）用の shadPS4 または GoldHEN のパッチファイル（.xml）を patches フォルダに入れると、そのパッチがここに表示され、オン・オフを切り替えられます。この移植版自体のパッチ（グラフィックス、フレームレート、チート）は内蔵されています。",
             zh: "把适用于 Bloodborne（应用版本 01.09）的 shadPS4 或 GoldHEN 补丁文件（.xml）放进 patches 文件夹：其中的补丁会显示在这里，可以开启或关闭。本移植版自带的补丁（画面、帧率和作弊）已内置。"),
        "Open Patches Folder":
            (ja: "パッチフォルダを開く",
             zh: "打开补丁文件夹"),
        "1 file":
            (ja: "1 ファイル",
             zh: "1 个文件"),
        "%@ files":
            (ja: "%@ ファイル",
             zh: "%@ 个文件"),
        "No mods or cheats":
            (ja: "Mod・チートなし",
             zh: "未启用模组或作弊"),
        "1 mod":
            (ja: "Mod 1 個",
             zh: "1 个模组"),
        "%@ mods":
            (ja: "Mod %@ 個",
             zh: "%@ 个模组"),
        "1 cheat":
            (ja: "チート 1 個",
             zh: "1 项作弊"),
        "%@ cheats":
            (ja: "チート %@ 個",
             zh: "%@ 项作弊"),
        "%@ on":
            (ja: "%@ が有効",
             zh: "已启用 %@"),
        "Could not save %@: %@":
            (ja: "%@ を保存できませんでした：%@",
             zh: "无法保存 %@：%@"),
        "Move “%@” to the Trash?":
            (ja: "「%@」をゴミ箱に入れますか？",
             zh: "要把“%@”移到废纸篓吗？"),
        "The mod's folder goes to the Trash; you can put it back from there.":
            (ja: "Mod のフォルダがゴミ箱に入ります。ゴミ箱から元に戻せます。",
             zh: "模组的文件夹会移到废纸篓；你可以从那里放回原处。"),
        "Move to Trash":
            (ja: "ゴミ箱に入れる",
             zh: "移到废纸篓"),
        "Could not move “%@” to the Trash: %@":
            (ja: "「%@」をゴミ箱に入れられませんでした：%@",
             zh: "无法把“%@”移到废纸篓：%@"),
        "Choose a mod: its folder or a .zip. It is copied into the mods folder.":
            (ja: "Mod を選択してください（フォルダまたは .zip）。mods フォルダにコピーされます。",
             zh: "请选择一个模组：它的文件夹或 .zip。它会被复制到 mods 文件夹。"),
        "Add":
            (ja: "追加",
             zh: "添加"),
        "Not added: ":
            (ja: "追加されませんでした：",
             zh: "未添加："),
        "“%@” can't be loaded as a mod":
            (ja: "「%@」は Mod として読み込めません",
             zh: "“%@”无法作为模组加载"),
        "A mod holds the game's folders (dvdroot_ps4, or chr, parts, map... directly). Mods that replace eboot.bin, sce_module or sce_sys are refused.":
            (ja: "Mod にはゲームのフォルダ（dvdroot_ps4、または chr、parts、map など）が入っている必要があります。eboot.bin、sce_module、sce_sys を置き換える Mod は受け付けません。",
             zh: "模组应包含游戏的文件夹（dvdroot_ps4，或直接包含 chr、parts、map 等）。替换 eboot.bin、sce_module 或 sce_sys 的模组会被拒绝。"),
        "Keep":
            (ja: "残す",
             zh: "保留"),
        // About
        "macOS port of the native Bloodborne PC port: the original PS4 game code runs through Rosetta 2, the graphics on Metal.":
            (ja: "Bloodborne ネイティブ PC 移植版の macOS 移植です。PS4 版のオリジナルのゲームコードを Rosetta 2 で実行し、グラフィックスは Metal で描画します。",
             zh: "Bloodborne 原生 PC 移植版的 macOS 移植：原版 PS4 游戏代码通过 Rosetta 2 运行，画面由 Metal 渲染。"),
        "Built on bbport by deadinside28, the Windows port by Mrsuss60 and the shadPS4 video core.":
            (ja: "deadinside28 氏の bbport、Mrsuss60 氏の Windows 移植版、shadPS4 のビデオコアをもとにしています。",
             zh: "基于 deadinside28 的 bbport、Mrsuss60 的 Windows 移植版以及 shadPS4 的视频核心。"),
        "Commit":
            (ja: "コミット",
             zh: "提交"),
        "Source code":
            (ja: "ソースコード",
             zh: "源代码"),
        "Releases":
            (ja: "リリース",
             zh: "发布版本"),
        "You need your own copy of the game. Not affiliated with FromSoftware or Sony Interactive Entertainment.":
            (ja: "ご自身で所有するゲームが必要です。FromSoftware および Sony Interactive Entertainment とは関係ありません。",
             zh: "你需要拥有自己的游戏副本。与 FromSoftware 和 Sony Interactive Entertainment 无关。"),
    ]
}
