# my-cs-go

一个面向 Windows x64 的 Vulkan 战术射击游戏工程骨架，目标是逐步演进成类似 CSGO 的项目，并预留单机、联机、地图编辑器与未来新模式扩展能力。

当前仓库提供的是第一版基础设施，重点放在工程边界、模块划分、内容格式和占位资源生成，而不是一次性做完完整 FPS。

## 当前已落地

- Vulkan 渲染器启动骨架，完成实例创建与设备枚举。
- UI 已切到 Vulkan 内部绘制，中文文字使用 FreeType 预烘焙字形图集，不再依赖运行机上的字体/GDI 叠字是否正常。
- 中文主菜单数据模型，包含单机、联机、地图编辑器、设置、退出等入口。
- 设置系统，支持图像、音频、操作、网络配置保存。
- 内容系统，内置步枪、狙击枪、冲锋枪、散弹枪、近战武器、手雷、闪光弹、烟雾弹。
- 瞄准镜槽位，支持红点、2 倍、4 倍、8 倍。
- 单机训练场模拟世界骨架。
- 联机层抽象，包含离线回环传输和 UDP 传输基础实现。
- 地图数据格式、地图编辑器逻辑、示例地图导出。
- 代码生成占位模型和贴图文件，便于后续替换成正式资产。

## 目录结构

- `src/app`: 应用启动、主菜单、设置、应用状态流转。
- `src/content`: 武器、材质、可替换资源绑定。
- `src/gameplay`: 地图格式、地图编辑器、玩法规则、模拟世界。
- `src/network`: 单机/联机共享的网络接口与传输层。
- `src/platform`: 平台窗口层，当前包含 Win32 和空实现。
- `src/renderer`: 渲染接口与 Vulkan 后端。
- `assets/maps`: 地图文件。
- `assets/generated`: 代码生成的模型、材质、贴图和预览。
- `assets/source`: 外部下载的正式资源，目前已接入部分 Poly Haven 材质与道具模型。

## Windows x64 构建

前提：

- 安装 Visual Studio 2022 或更新版本，带 MSVC x64 工具链。
- 安装 Vulkan SDK，并确保 `VULKAN_SDK` 可用。
- 使用支持 `CMakePresets.json` 的 CMake。

命令：

```powershell
cmake --preset windows-vs2022-x64
cmake --build --preset windows-debug
```

## 无 root 的本地交叉编译依赖

当前仓库已经支持把交叉编译依赖放在 `.local-tools/` 下使用，不需要系统安装。

本地已准备：

- `CMake 4.3.0`
- `Ninja 1.13.2`
- `llvm-mingw 20260311`
- `Vulkan-Headers 1.4.341.0`

交叉编译命令：

```bash
./tools/configure-windows-cross.sh
./tools/build-windows-cross.sh
```

构建完成后，把 `build/windows-x64-llvm-mingw/` 整个目录一起拷到 Windows。

目录里除了 `my-cs-go.exe`，还会自动带上运行时 DLL：

- `libc++.dll`
- `libunwind.dll`
- `libwinpthread-1.dll`

## 中文文字资源生成

当前中文 UI 文本走的是“构建时烘焙，运行时直接绘制”的方案：

- `tools/font_atlas_baker.cpp`：使用 FreeType 读取字体并导出字形数据。
- `tools/ui_font_charset.txt`：定义需要进入图集的字符集合。
- `tools/generate-ui-font.sh`：生成 `src/renderer/font/GeneratedUiFontData.h/.cpp`。
- `tools/build-windows-cross.sh`：会在正式编译前自动执行一次字形生成。

Linux 侧需要准备：

- `freetype2` 开发包
- `Noto Sans CJK` 字体，脚本默认查找系统里的 `NotoSansCJK-Regular.ttc` 或 `NotoSansCJK-Bold.ttc`

这个方案的好处是：

- Windows 运行时不依赖目标机器是否装了特定中文字体。
- 不依赖 GDI 叠字和桌面合成是否在某台机器上表现一致。
- 后续只要更新字符集或替换源字体，再重新构建一次即可。

## 外部资源

当前仓库已经下载并接入一批外部资源，目录在：

- `assets/source/polyhaven/materials`
- `assets/source/polyhaven/models`

已落地：

- `concrete_wall_006`
- `concrete_floor`
- `painted_concrete`
- `wooden_crate_02`
- `Barrel_02`

当前接入方式分两层：

- 资源绑定层已经改为指向这些正式文件。
- 单机训练场已经把箱子和油桶从“整格方块”升级成独立道具渲染。

当前仍然是轻量原型集成，还没有完整的通用 glTF 网格渲染管线。

## 当前限制

- 还没有完整的 Vulkan 网格/模型/材质渲染管线，当前 UI 文本已可用，但整体画面仍以原型验证为主。
- 联机层已具备结构和 UDP 基础，但还没有做权威服务器、回滚同步、命中校验。
- 地图编辑器当前实现的是数据层和预览导出，还没有进入真正的场景内编辑交互。
- 武器系统目前是数据定义和扩展点，还没有完整射击、后坐力、弹道、爆炸与烟雾体积效果。

## 后续建议路线

1. 优先补全 Win32 输入、Vulkan swapchain、基础几何渲染和相机。
2. 将主菜单与设置接入真正的 UI 渲染。
3. 用 ECS 或轻量组件表重构模拟层，方便未来扩展大逃杀模式。
4. 将 `MapEditor` 接到场景内 gizmo/刷子操作，实现真正的可视化编辑。
5. 在 `NetworkSession` 上叠加服务器权威同步与房间管理。
