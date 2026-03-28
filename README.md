# my-cs-go

一个面向 Windows x64 的 Vulkan 战术射击游戏原型工程，目标是逐步演进成类似 CSGO 的项目，并预留单机、联机、地图编辑器与未来新模式扩展能力。

当前仓库已经不再只是“启动骨架”，而是包含可运行的游戏程序、独立地图编辑器、资源清单与对象资产管理、基础联机流程，以及一条持续演进中的 Vulkan 网格渲染路径。

## 当前已落地

- Vulkan 渲染器、交换链重建、基础网格绘制、实例化绘制与 ImGui 工具界面。
- 中文 UI 已切到打包内字体与 ImGui，不再依赖运行机上的字体/GDI 叠字是否正常。
- 中文主菜单数据模型，包含单机、联机、地图编辑器、设置、退出等入口。
- 设置系统，支持图像、音频、操作、网络配置保存。
- 内容系统，内置步枪、狙击枪、冲锋枪、散弹枪、近战武器、手雷、闪光弹、烟雾弹。
- 瞄准镜槽位，支持红点、2 倍、4 倍、8 倍。
- 单机训练场、基础角色、武器切换、跳跃、鼠标视角与 Jolt Physics 驱动的人物移动/碰撞。
- 联机层抽象与基础房间流程，已支持主机/客户端进入同一局游戏的原型链路。
- 独立地图编辑器 `my-cs-go-editor.exe`，支持自由镜头、2.5D 视图、选择/放置/擦除、对象参数编辑、撤销和对象可视化开关。
- 地图数据、对象资产清单、资源清单和示例地图导出。
- 代码生成占位模型和贴图文件，且已接入部分外部 glTF/OBJ 资源，便于后续继续替换成正式资产。

## 目录结构

- `src/app`: 应用启动、主菜单、设置、应用状态流转。
- `src/content`: 武器、材质、可替换资源绑定。
- `src/gameplay`: 地图格式、地图编辑器、玩法规则、模拟世界。
- `src/network`: 单机/联机共享的网络接口与传输层。
- `src/platform`: 平台窗口层，当前包含 Win32 和空实现。
- `src/renderer`: 渲染接口与 Vulkan 后端。
- `assets/maps`: 地图文件。
- `assets/generated`: 代码生成的模型、材质、贴图、shader 与 mesh cache。
- `assets/source`: 外部下载并实际接入的源资源。

## 运行与发布

当前 Windows 发布物有两个可执行文件：

- `my-cs-go.exe`：游戏主程序
- `my-cs-go-editor.exe`：独立地图编辑器

GitHub Releases 当前提供两类包：

- `my-cs-go-windows-x64-vX.Y.Z.zip`
  这是完整包，包含运行时 DLL 与完整 `assets` 目录，体积较大，适合调试或保留全部资源。
- `my-cs-go-windows-x64-vX.Y.Z-runtime.zip`
  这是精简运行包，只保留当前运行所需资源，体积更小，优先推荐下载。

两种包解压后都可以直接在 Windows x64 上运行。

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

## 系统安装的交叉编译依赖

当前仓库优先使用系统里安装的交叉编译工具，不再依赖 `.local-tools/` 里的固定路径。

Ubuntu 24.04 可以安装：

- `cmake`
- `ninja-build`
- `glslang-tools`
- `libvulkan-dev`
- `mingw-w64`
- `gcc-mingw-w64-x86-64`
- `g++-mingw-w64-x86-64`
- `binutils-mingw-w64-x86-64`

可以直接执行：

```bash
sudo apt update
sudo apt install -y \
  build-essential \
  cmake \
  ninja-build \
  glslang-tools \
  libvulkan-dev \
  mingw-w64 \
  gcc-mingw-w64-x86-64 \
  g++-mingw-w64-x86-64 \
  binutils-mingw-w64-x86-64 \
  pkg-config \
  libfreetype-dev \
  fonts-noto-cjk \
  python3 \
  rsync
```

交叉编译命令：

```bash
./tools/configure-windows-cross.sh
./tools/build-windows-cross.sh
```

构建完成后，把 `build/windows-x64-mingw/` 整个目录一起拷到 Windows。

目录里除了 `my-cs-go.exe`，还会自动带上运行时 DLL。常见会包含：

- `libstdc++-6.dll` 或 `libc++.dll`
- `libgcc_s_seh-1.dll` 或 `libunwind.dll`
- `libwinpthread-1.dll`
- `SDL3.dll`

如果想生成方便分发的 Windows 目录，可以使用：

```bash
./tools/copy-windows-package.sh
```

默认会把运行时文件和 `assets/` 同步到 `dist/windows-x64-mingw/`，也可以手动指定目标目录。

## 中文文字资源生成

当前中文 UI 文本主要走两条链路：

- 游戏内原型 HUD 与位图文字：构建时烘焙字形图集。
- ImGui 工具界面：优先使用 `assets/fonts/NotoSansSC-Variable.ttf`，找不到时再退回系统字体。

相关工具：

- `tools/font_atlas_baker.cpp`：使用 FreeType 读取字体并导出字形数据。
- `tools/ui_font_charset.txt`：定义需要进入图集的字符集合。
- `tools/generate-ui-font.sh`：生成 `src/renderer/font/GeneratedUiFontData.h/.cpp`。
- `tools/build-windows-cross.sh`：会在正式编译前自动执行一次字形生成。

Linux 侧需要准备：

- `freetype2` 开发包
- 如果仓库里没有 `assets/fonts/NotoSansSC-Variable.ttf`，再准备 `Noto Sans CJK` 系统字体
- 也可以手动设置 `FONT_PATH=/path/to/font.ttf` 指定字形生成源字体

这个方案的好处是：

- Windows 运行时不依赖目标机器是否装了特定中文字体。
- 不依赖 GDI 叠字和桌面合成是否在某台机器上表现一致。
- 后续只要更新字符集或替换源字体，再重新构建一次即可。

## 外部资源

当前仓库已经接入一批外部资源，目录主要在：

- `assets/source/polyhaven/materials`
- `assets/source/polyhaven/models`
- `assets/source/weapons/quaternius_ultimate_guns/OBJ`
- `assets/source/itchio/metro_psx`
- `assets/source/itchio/universal_base_characters_standard`

已落地：

- `concrete_wall_006`
- `concrete_floor`
- `painted_concrete`
- `wooden_crate_02`
- `Barrel_02`
- `Quaternius Ultimate Guns` 中的一批 OBJ 武器模型
- `metro_psx` 地铁站场景资源
- `Universal Base Characters` 中的男女角色 glTF 资源

当前接入方式分两层：

- 资源绑定层已经改为指向这些正式文件。
- 游戏与地图编辑器已经能直接引用对象资产和材质配置。

当前仍然是轻量原型集成，渲染路径正在继续向更完整的通用 glTF/OBJ 运行时演进。

## 当前限制

- 联机还是原型阶段，尚未实现真正的专用权威服务器、回滚同步、命中校验和 ENet 集成。
- 地图编辑器已经可用，但仍缺少更成熟的 gizmo、吸附、批量操作和更完整的资产管理工作流。
- 渲染器虽然已经能跑正式网格资源，但材质系统、动画、光照和更专业的资源缓存仍需继续工程化。
- 武器与玩法已经有基础原型，但距离完整 FPS 体验仍有明显差距，例如更完整的后坐力、弹道、投掷物与特效系统。

## 后续建议路线

1. 继续完善正式网格/材质渲染路径，把更多资源从原型 fallback 切到稳定缓存与批处理流程。
2. 用 ECS 持续重构模拟层，方便以后扩展大逃杀、载具、更多投掷物和 AI。
3. 在 `NetworkSession` 基础上继续推进真正的联机同步、房间管理与更稳的传输层。
4. 继续完善 `my-cs-go-editor.exe`，补齐更成熟的资产管理、操作器、吸附与批量编辑能力。
5. 逐步把角色、武器、音频和地图资源链路做成更标准的工程化内容管线。
