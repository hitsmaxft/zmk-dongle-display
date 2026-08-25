# Fighter 动画主题开发方案

## 1. 目标与边界

基线为 `custom_anima@add9f1b`，开发分支为 `codex/fighter-animation-theme`。本次只扩展通用动画引擎和 128×64 OLED 布局，不改变未启用动画扩展时的原始 Bongo Cat 路径。

目标行为：

- Fighter 作为内置 animation pack，可通过现有 `animation-next` 行为轮换选择；`fighter-next` 暂时保留为兼容别名。
- Fighter 的 idle/slow 保持普通双栏状态屏；mid/fast 进入全屏战斗模式。
- mid/fast 隐藏普通状态组件，但显示 KOF96 风格的左右手电池 HUD。
- 位移只跟随实际图片帧推进，不跟随 LVGL 刷新 tick；最后一帧到达目标，完整显示一个帧周期后复位原坐标。
- mid 每周期向左移动约屏宽的 1/3；fast 每周期移动至左边缘，所有坐标都不得超出屏幕。

不在本次范围：动画在线传输、动态加载、灰阶图、全彩屏适配、运行时修改 WPM 阈值。

## 2. 建议架构

屏幕拆成三个同级 layer：

| Layer | 普通模式 | Fighter mid/fast | 内容 |
|---|---:|---:|---|
| `normal_layer` | 显示 | 隐藏 | 输出、修饰键、层、HID 指示、普通电池组件 |
| `animation_layer` | 显示 | 显示 | 当前 animation pack 的图像对象 |
| `battle_hud_layer` | 隐藏 | 显示 | 左右手数字框和镜像电池血条 |

`animation.c` 负责 action、帧、位移和模式切换；`custom_status_screen.c` 只创建 layer 并将它们交给动画 widget，不根据 WPM 编写 fighter 特例。

## 3. Provider ABI v2

在 `include/zmk/dongle_display/animation.h` 中将 ABI 升为 2，并向 action 增加稳定宽度字段，避免直接把 C enum 放进 ABI：

```c
enum zmk_dongle_animation_motion {
    ZMK_DONGLE_ANIMATION_MOTION_NONE = 0,
    ZMK_DONGLE_ANIMATION_MOTION_LEFT_SCREEN_THIRD = 1,
    ZMK_DONGLE_ANIMATION_MOTION_LEFT_EDGE = 2,
};

#define ZMK_DONGLE_ANIMATION_FLAG_FULLSCREEN BIT(0)
#define ZMK_DONGLE_ANIMATION_FLAG_BATTLE_HUD BIT(1)

struct zmk_dongle_animation_action {
    const char *name;
    const void *const *frames;
    uint8_t frame_count;
    uint32_t duration_ms;
    uint8_t motion;
    uint8_t flags;
};
```

保留现有 `ZMK_DONGLE_ANIMATION_ACTION_DEFINE`，让它默认生成 `motion=NONE, flags=0`；新增 `ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_DEFINE` 供 fighter 使用。这样现有 Provider 只需要重新编译，不必为每个 action 增加参数。若外部生成器直接输出结构体初始化，则必须同步到 ABI v2。

校验增加：

- `motion` 只能取已知值，`flags` 不能包含未知 bit。
- 位移动画至少 2 帧，且 `duration_ms >= frame_count`。
- widget 初始化后根据真实屏宽计算距离，并检查 `frame_count - 1 <= distance_px`；否则返回 `-ERANGE`，因为“每帧至少 1px”和“终点不越界”无法同时满足。
- 每个 frame 的图像尺寸必须不大于 registry canvas；建议内置 fighter 所有帧统一为 50×26。

## 4. 按真实帧数计算位移

设 action 有 `N` 帧，右侧原坐标为 `x0`，目标为 `x1`，总距离 `D = x0 - x1`。第 `i` 帧位置：

```c
x(i) = x0 - ((D * i + (N - 1) / 2) / (N - 1)), i = 0..N-1
```

累计取整会把余数均匀分散，且当 `D >= N - 1` 时，相邻图片帧至少移动 1px。128×64、50px canvas 的参考坐标：

| Action | 起点 | 终点 | 距离 |
|---|---:|---:|---:|
| idle/slow | 78 | 78 | 0 |
| mid | 78 | `max(0, 78-round(128/3)) = 35` | 43px |
| fast | 78 | 0 | 78px |

不要继续使用两个独立的 `lv_animimg`/坐标动画，否则帧源和坐标可能在不同 tick 更新。建议改成 `lv_image` 加一个 `lv_timer`：

1. action 启动时立即显示 frame 0 和 `x0`。
2. 每次 timer 回调只推进一个实际 frame，同时更新 image source 和 `x(i)`。
3. 基本帧周期为 `duration_ms / N`，余数 `duration_ms % N` 均匀分配到各帧，保证整个 action 总时长不漂移。
4. 最后一帧在目标位置保持自己的完整帧周期。
5. 下一次 timer 回调进入 completion：先解析 pending theme/WPM，再把动画对象恢复到 `x0` 并显示下一 action 的 frame 0。
6. 同一 mid/fast 重播时保持 battle HUD 可见，只复位动画坐标，不闪现普通状态层。

WPM 升档可以沿用当前“立即打断并进入更高 band”的语义，但打断时先复位到 `x0`；降档和主题切换仍等待当前 action 完成。

建议把位置、周期和目标计算放进 `animation_math.h`，保持纯函数，便于 host/ztest 覆盖。

## 5. Fighter pack 与资源

在默认 Provider 中注册 `bongo_pack, fighter_pack`，通过现有 pack index 轮换。自定义 Provider 模式仍由外部 registry 完全接管，不自动混入内置 pack，避免重复 symbol 和不可控 Flash 增量。

推荐首版资源预算：

| Action | 帧数 | 建议总时长 | 行为 |
|---|---:|---:|---|
| idle | 4 | 800ms | 原地呼吸/架势 |
| slow | 6 | 600ms | 原地轻动作 |
| mid | 8 | 480ms | 跑动，43px 均匀位移 |
| fast | 8 | 280ms | 冲刺，78px 均匀位移 |

使用严格 1-bit、统一 50×26 canvas。资源建议放在 `widgets/fighter_images.c`，声明放在 `fighter_images.h`；不要生成 128×64 全屏帧，否则每帧约 1KB，浪费 Flash。位移由对象坐标完成。

## 6. KOF 风格电池 HUD

新增 `widgets/battle_battery.c/.h`，只订阅 `zmk_peripheral_battery_state_changed`，忽略 dongle 自身电池。默认 `source 0=左手、source 1=右手`，增加两个 Kconfig 整数用于覆盖映射。

推荐 128px 横向几何：

| 元素 | 范围 | 说明 |
|---|---|---|
| 左数字框 | x=0..23 | 24×11，显示 `0..100`，无 `%` |
| 左血条 | x=25..60 | 36×7，从左侧数字框向中心填充 |
| 中央留白 | x=61..66 | 6px |
| 右血条 | x=67..102 | 36×7，从右侧数字框向中心镜像填充 |
| 右数字框 | x=104..127 | 24×11 |

细框 1px，框内留 1px 空隙，白色实心条宽度按 `floor(level * inner_width / 100)` 计算并钳制到 0..100。数字使用现有 `lv_font_unscii_8`、letter spacing 0；从未收到电量的手显示 `--` 和空条。

为减少 SRAM，不创建 128×12 的 L8 canvas。使用 LVGL rectangle/label 对象组成 HUD；需要黑色底时只给两个数字框和血条容器设置黑色背景，保持其余屏幕与现有 1-bit 图像兼容。

HUD 固定在顶部，fighter 的 fullscreen action 底部对齐，50×26 canvas 对应 `y=38`，避免与 11px HUD 重叠。每次 action 切换后将 HUD 提到前景。

## 7. 文件改动清单

| 文件 | 修改内容 |
|---|---|
| `include/zmk/dongle_display/animation.h` | ABI v2、motion/flags、新 action 宏 |
| `widgets/animation.h/.c` | layer 引用、单 timer 帧状态机、位移/复位、模式切换 |
| `widgets/animation_math.h` | 目标坐标、逐帧坐标、帧周期余数算法 |
| `widgets/animation_provider.c` | 注册 fighter pack，Bongo 保持默认布局 |
| `widgets/fighter_images.c/.h` | 1-bit fighter 帧资源 |
| `widgets/battle_battery.c/.h` | 左右手战斗 HUD 和电量事件 |
| `custom_status_screen.c` | 创建 normal/animation/battle HUD 三层 |
| `Kconfig.defconfig` | 左右 source 映射、可选 fighter pack 开关 |
| `boards/.../CMakeLists.txt` | 条件编译新 widget 和资源 |
| `README.md` | 主题切换、ABI v2、source 映射和全屏行为 |
| `behavior_fighter_next.c`/binding | 保留兼容别名并在文档标注，后续版本再删除 |

## 8. 实施顺序

1. 先完成 ABI v2、兼容宏和纯数学函数测试，不改 UI。
2. 将动画执行器改成单 timer，并用现有 Bongo pack 回归帧序、时长、NEXT 和 WPM 升降档。
3. 引入 fighter 资源和 pack，验证每个 action 的 Flash 增量及真实帧数约束。
4. 拆分三个 screen layer，完成 fullscreen/normal 原子切换和坐标复位。
5. 实现 battle HUD、电量 source 映射和未知状态。
6. 更新构建矩阵、文档，最后上 128×64 实屏调尺寸和 OLED 可见帧率。

## 9. 验收矩阵

- 未启用 `ANIMATION_EXTENSION`：产物和原始 Bongo 行为不变。
- 启用默认 extension：Bongo 与 Fighter 可轮换，NEXT 在当前 action 完成后生效。
- 自定义 Provider：旧宏源码可重新编译；ABI 1 或非法 layout 明确失败。
- Fighter idle/slow：普通状态层显示，HUD 隐藏，动画在右侧原坐标。
- Fighter mid：普通状态层隐藏，HUD 显示，逐帧从 x=78 到 x=35，完成后复位 x=78。
- Fighter fast：逐帧从 x=78 到 x=0，不出现负坐标，完成后复位 x=78。
- mid/fast 连续循环：HUD 不闪烁；每周期第一帧都在原坐标。
- mid→fast 打断、fast→slow 降档、播放中 NEXT、休眠唤醒轮换均无错误 layer 残留。
- 左右电量 0/1/50/99/100、未知 source、仅一个 peripheral、source 映射反转均正确。
- 记录 `.text/.rodata/.bss` 增量；HUD 不引入全屏 L8 buffer，显示线程栈无明显增长。


