<img src="web/logo.png" width="80" height="80" alt="Vivaldi++ Logo">

# Vivaldi Plus

[![nightly build](https://github.com/ca-x/vivaldi_plus/actions/workflows/nightly.yml/badge.svg)](https://github.com/ca-x/vivaldi_plus/actions/workflows/nightly.yml)
[![release build](https://github.com/ca-x/vivaldi_plus/actions/workflows/release.yml/badge.svg)](https://github.com/ca-x/vivaldi_plus/actions/workflows/release.yml)

[**中文**](#中文) | [**English**](#english)

---

## 中文

### 功能特性
- **便携设计** - 程序放在App目录，数据放在Data目录（不兼容原版数据，可以重装系统换电脑不丢数据）
- **移除更新警告** - 因为是绿色版没有自动更新功能，移除更新错误警告
- **密码便携化** - 密码数据可跨机器使用，不绑定硬件ID

> **注意：** 本程序已经移除之前的标签页增强功能，因为Vivaldi大部分功能已经内置，现在只保留便携化核心功能：
> - ~~双击鼠标中键关闭标签页~~
> - ~~保留最后标签页~~
> - ~~鼠标悬停标签栏滚动~~
> - ~~按住右键时滚轮滚动标签栏~~

### 自定义配置

#### 网页设置（默认关闭）

网页设置为可选功能。没有 `config.ini`，或旧配置没有 `webui`，都按 `webui=0` 处理：
不启动本地服务，不显示关于页信息；以前启用过的自有入口会被移除。原有便携化功能继续运行。

要开启，在 DLL 同目录的 `config.ini` 中加入：

```ini
[general]
webui=1
```

完全退出**所有**浏览器实例后重新启动。设置回 `0` 可关闭。

打开 `vivaldi:about`，在“系统与构建”中点击 **Vivaldi++ → 设置**，入口跟随浏览器语言。
Chrome 对应入口在 `chrome://settings/help`（设置 → 关于 Chrome）。
网页、Logo、样式、脚本全部嵌入 `version.dll`；DLL 在主浏览器进程中启动本地服务，
通常位于 `http://127.0.0.1:16877/settings.html`（端口占用时尝试 16878–16887）。
网页会自动加载 DLL 同目录的 `config.ini`，支持中文/英文、浅色/深色/跟随系统，
以及配置导入导出。语言和主题记忆在浏览器中，与启动配置独立。

点击保存会原子写入原来的 INI，保留未知配置和注释；检测到文件被外部修改时拒绝覆盖。
没有配置文件时按默认值显示，第一次保存时创建 UTF-16LE 文件以支持 Windows INI 的中文路径。
更改数据目录不会移动现有数据。**保存后完全退出并重新启动 Vivaldi 才会生效。**

关于页入口由 C++ 直接集成：Vivaldi 只修改其界面的一处入口；Chrome 只修改资源包内关于页模板。
不释放或执行 PowerShell/C#。关闭时仅删除自己的入口，保留其他资源。
网页和 Logo 直接从 DLL 资源提供，不释放到安装目录。
只在能识别且可写时更新入口；不创建备份或安装记录，重复启动且入口相同时不会重写文件。
遇到不支持的界面版本或外部修改时保留原文件，不强行修改。
创建 `App/vivaldi-plus-about-disabled` 可禁止自动集成，删除该标记后恢复。

#### Chrome 兼容

提供 `vivaldi-windows-*.zip` 和 `chrome-windows-*.zip` 两套包，共用原生 DLL，按实际浏览器识别入口。
将 DLL 放在 `vivaldi.exe` 或 `Chrome.exe` 同目录，统一使用本项目的 `config.ini`。
Chrome 支持便携数据/缓存目录、启动参数、老板键，以及显式开启后的网页设置。
其他 Chrome++ 项目的 `chrome++.ini` 和标签增强选项不会自动迁移；不要混用两套 DLL。

本地服务只监听 IPv4 回环地址。配置接口固定指向本安装的 `config.ini`，
校验 Host、会话令牌、写入来源，不对局域网提供访问，也不支持任意文件读取或执行命令。
同安装的多个实例共用一个服务、端口和会话；服务所属实例退出后由其他存活实例接管。
未发现可集成的关于页入口时不启动设置服务。

本DLL提供配置选项，在DLL同目录新建 `config.ini` 文件即可配置。

#### 快速开始
1. 复制 `config.ini.example.zh-CN` 为 `config.ini`
2. 根据需要修改配置项（通常使用默认值即可）
3. 重启浏览器

#### 配置文件说明

**完整配置示例：**
```ini
[general]
# 网页设置默认关闭；没有此项也按 0 处理
webui=0

# Win32k 系统调用策略
# win32k=0 (推荐) - 保持默认沙箱，确保 GPU 硬件加速和视频流性能
# win32k=1 (兼容) - 仅当 Chrome 启动崩溃时使用，可能影响视频播放质量
win32k=0

# 调试日志
# debug_log=0 (默认) - 不输出调试日志
# debug_log=1 (故障排除) - 输出调试日志 (使用 DebugView 查看)
debug_log=0

# Chrome 禁用特性
# 留空使用默认值 (推荐): WinSboxNoFakeGdiInit,WebUIInProcessResourceLoading
# 或自定义您需要的特性
disable_features=

# 额外的命令行参数
command_line=

[dir_setting]
# 数据目录（用户配置、书签、扩展等）
data=%app%\..\Data

# 缓存目录（临时文件）
# 性能提示: 建议放在 SSD 上以获得最佳视频流性能
cache=%app%\..\Cache

[hotkey]
# 老板键 - 快速隐藏/显示浏览器并静音/取消静音
# 格式: Ctrl+Alt+B 或 Win+H 等
# 留空则禁用此功能
boss_key=
```

#### 配置项详解

##### `[general]` 部分

- **`win32k`** (默认: `0`)
  - `0` - **推荐设置** 🟢
    - 保持 Chrome 默认沙箱安全策略
    - 确保 GPU 硬件加速正常工作
    - **视频流性能最佳** (Twitch, YouTube 等)
    - CPU 占用低 (10-20%)
  - `1` - **兼容模式** 🟡
    - 仅当浏览器启动崩溃时使用
    - 可能修复某些杀毒软件冲突 (如 Symantec, McAfee)
    - ⚠️ 可能导致视频硬件解码失败
    - ⚠️ Twitch/YouTube 可能播放质量降低
    - ⚠️ CPU 占用增加 (50-80%)

- **`debug_log`** (默认: `0`)
  - `0` - 不输出调试日志，性能影响最小
  - `1` - 输出调试日志用于故障排除
    - 通过 OutputDebugString 输出（使用 [DebugView](https://learn.microsoft.com/en-us/sysinternals/downloads/debugview) 查看）
    - 记录命令行参数、错误和便携模式操作
    - 仅在调查问题时使用

- **`command_line`** (默认: 空)
  - 额外的 Chrome 命令行参数
  - 示例: `command_line=--force-dark-mode --enable-features=WebUIDarkMode`

- **`disable_features`** (默认: `WinSboxNoFakeGdiInit,WebUIInProcessResourceLoading`)
  - 指定要禁用的 Chrome 特性
  - **留空或不指定**: 使用默认兼容性特性 (推荐)
  - **指定值**: 使用您的自定义特性 (替换默认值)
  - 常用特性:
    - `WinSboxNoFakeGdiInit` - 修复 GPU 初始化问题
    - `WebUIInProcessResourceLoading` - 改善 WebUI 兼容性
    - `RendererCodeIntegrity` - 允许 DLL 注入
    - `AutofillServerCommunication` - 禁用自动填充同步
    - `TranslateUI` - 禁用翻译提示
  - 示例:
    ```ini
    # 使用默认 (推荐)
    disable_features=

    # 自定义特性
    disable_features=RendererCodeIntegrity,TranslateUI

    # 添加更多特性
    disable_features=WinSboxNoFakeGdiInit,WebUIInProcessResourceLoading,TranslateUI
    ```

##### `[dir_setting]` 部分

- **`data`** (默认: `%app%\..\Data`)
  - 用户数据目录（配置文件、书签、扩展等）
  - 支持格式:
    - 相对路径: `data=..\Data`
    - 绝对路径: `data=C:\MyBrowser\Data`
    - 环境变量: `data=%LOCALAPPDATA%\VivaldiPlus\Data`
    - `%app%` 变量: `data=%app%\..\Data`

- **`cache`** (默认: `%app%\..\Cache`)
  - 缓存目录（临时文件、媒体缓存等）
  - **性能提示**:
    - 建议放在 SSD 上（非机械硬盘）
    - 确保至少 2GB 剩余空间
    - 避免网络驱动器

##### `[hotkey]` 部分

- **`boss_key`** (默认: 空，禁用)
  - 老板键 - 快速隐藏/显示浏览器并静音/取消静音
  - **功能**:
    - 按下热键立即隐藏所有 Vivaldi 窗口并静音所有音频
    - 再次按下恢复窗口并取消静音
    - 全局热键（系统范围有效）
  - **格式**: `修饰键+修饰键+按键`
    - 修饰键: `Ctrl` (或 `Control`)、`Alt`、`Shift`、`Win`
    - 按键: `A-Z`、`0-9`、`F1-F24`、方向键、`Esc`、`Tab`、`Space` 等
  - **示例**:
    ```ini
    boss_key=Ctrl+Alt+B
    boss_key=Ctrl+Shift+H
    boss_key=Win+H
    boss_key=Ctrl+`
    ```
  - **注意事项**:
    - 确保热键不与其他应用程序或系统热键冲突
    - 留空则禁用老板键功能
    - 音频静音状态会被保留（仅在原本未静音时才取消静音）

#### 故障排除

**问题**: Twitch/YouTube 视频加载慢或质量低 (160p)
- **检查**: 打开 `chrome://gpu`，查看 "Video Decode" 是否为 "Hardware accelerated"
- **解决**: 确保 `config.ini` 中 `win32k=0` (或不存在 config.ini)

**问题**: 浏览器启动立即崩溃
- **解决**: 创建 `config.ini`，设置 `win32k=1`
- **建议**: 考虑将浏览器从杀毒软件扫描中排除

**问题**: 视频卡顿或频繁缓冲
- **检查**: 缓存目录位置和磁盘速度
- **解决**: 将 cache 目录移到 SSD：`cache=C:\Path\To\SSD\Cache`

**问题**: 老板键不起作用
- **解决**:
  1. 确保热键不与系统热键冲突
  2. 尝试不同的按键组合
  3. 设置 `debug_log=1` 并使用 DebugView 查看错误消息
  4. 确保按下的是配置中指定的确切组合

**问题**: 窗口恢复顺序错误或未显示所有窗口
- **说明**: 老板键以相反的创建顺序恢复窗口
- **解决**: 尝试再次隐藏和显示，或重启浏览器

详细配置说明请查看 `config.ini.example.zh-CN` 文件。

### 架构支持

Vivaldi Plus 现在支持多种 CPU 架构：

- **x86** (32位) - 传统 32 位 Windows 系统
- **x64** (64位) - 现代 64 位 Windows 系统
- **ARM64** - Windows on ARM 设备（如 Surface Pro X、Snapdragon 笔记本等）

#### 为什么使用 Microsoft Detours？

随着 Vivaldi 浏览器推出 ARM64 版本以支持 Windows on ARM 设备，原有的 MinHook 库无法满足需求，因为 **MinHook 不支持 ARM64 架构**。

为了让 Vivaldi Plus 能够在所有架构上运行，我们已将 hook 库从 MinHook 迁移到 [Microsoft Detours](https://github.com/microsoft/Detours)：

- ✅ **完整的架构支持** - Detours 原生支持 x86、x64 和 ARM64
- ✅ **微软官方维护** - 由 Microsoft 开发和维护，稳定可靠
- ✅ **成熟的生态** - 被广泛应用于 Windows 平台的各类工具中
- ✅ **更好的兼容性** - 与 Windows on ARM 完美兼容

现在，无论你使用什么架构的 Windows 设备，都可以享受 Vivaldi Plus 带来的便携化体验！

### 下载与安装

#### 获取Vivaldi浏览器
请访问 https://vivaldi.czyt.tech 获取下载链接

#### 获取Vivaldi Plus
- **稳定版本（Stable）**：从 [Releases](https://github.com/ca-x/vivaldi_plus/releases) 页面下载
- **开发版本（Nightly）**：[Powered by nightly.link](https://nightly.link/ca-x/vivaldi_plus/workflows/nightly/main) - 每次代码推送后自动构建

> **版本说明：**
> - **Stable (稳定版)**：经过测试的正式版本，推荐大多数用户使用
> - **Nightly (开发版)**：包含最新功能和修复，但可能不够稳定

#### 安装方法
将 `version.dll` 放入解压版Vivaldi目录（`vivaldi.exe` 同目录）即可

### 构建

```bash
# 安装 xmake
# 克隆仓库（注意使用 --recursive 参数以包含 Detours 子模块）
git clone --recursive https://github.com/ca-x/vivaldi_plus.git
cd vivaldi_plus

# 构建 x64 (推荐)
xmake f -a x64 -m release
xmake

# 构建 ARM64 (Windows on ARM)
xmake f -a arm64 -m release
xmake

# 构建 x86 (32位)
xmake f -a x86 -m release
xmake
```

编译后的 DLL 将输出到 `build/release/<架构>/version.dll`

### 源项目
基于 [chromePlus](https://github.com/icy37785/chrome_plus) 项目

> 修复 Chrome 118+ 的代码参考了 [Bush2021的项目](https://github.com/Bush2021/chrome_plus)，在此表示感谢。

---

## English

### Features
- **Portable Design** - Keep app in App folder and data in Data folder (incompatible with original data, but allows system reinstallation without data loss)
- **Remove Update Warnings** - Removes update error warnings since this is a portable version without auto-update
- **Password Portability** - Password data can be used across machines, not tied to hardware ID

> **Note:** Tab enhancement features have been removed as most are now built into Vivaldi. Only core portability features remain:
> - ~~Double-click middle button to close tab~~
> - ~~Keep last tab open~~
> - ~~Mouse hover tab scrolling~~
> - ~~Right-click + wheel to scroll tabs~~

### Configuration

#### Web settings (off by default)

Missing configuration or a missing `[general] webui` key is treated as `webui=0`.
The portable features continue to work, with no settings service or about-page information.
To enable the optional UI, set `webui=1` in `[general]` inside the adjacent `config.ini`,
then completely exit **all** browser instances and restart. Set it back to `0` to disable the service and remove its own entry.

Open `vivaldi:about` and follow **Vivaldi++ → Settings** under System and Build. The entry follows the browser language.
For Chrome, open `chrome://settings/help` (Settings → About Chrome).
All HTML, logo assets, CSS and JavaScript are embedded in `version.dll` and served directly from DLL resources.
The loopback service uses `http://127.0.0.1:16877/settings.html`, with ports 16878–16887 as fallbacks.
It loads the adjacent `config.ini` automatically. Chinese/English and light/dark/system themes are supported.
Saving retains unknown INI entries and comments, detects external edits, and writes UTF-16LE atomically.
Fully exit and restart Vivaldi to apply configuration changes. Changing directory settings does not move data.

The single about entry is integrated directly in C++, without extracting or executing installer scripts.
Vivaldi uses its UI bundle; Chrome uses the about template in its resource pack. No backups or installation records are created; an unchanged entry is not rewritten.
Unsupported UI bundles or external changes are left intact. No page assets are extracted to disk.
When a supported about-page entry exists, the settings service is available while the browser is running.
It binds only to `127.0.0.1` and protects its fixed-path configuration API with host, token and origin checks.
Instances from the same installation share one service and port, with takeover when the owner exits.
The settings service is not started when no supported about-page entry is found.

Chrome and Vivaldi packages share the native DLL and use this project's `config.ini`.
They support portable data/cache directories, startup arguments and the boss key.
Configuration and tab-enhancement options from other Chrome++ projects are not automatically migrated.

This DLL provides configuration options. Create a `config.ini` file in the same directory as the DLL.

#### Quick Start
1. Copy `config.ini.example` to `config.ini`
2. Modify configuration items as needed (defaults work for most cases)
3. Restart browser

#### Configuration File Format

**Complete configuration example:**
```ini
[general]
# 网页设置默认关闭；没有此项也按 0 处理
webui=0

# Win32k System Call Policy
# win32k=0 (recommended) - Keep default sandbox, ensure GPU acceleration and video streaming
# win32k=1 (compatibility) - Use only if Chrome crashes at startup, may affect video quality
win32k=0

# Debug Logging
# debug_log=0 (default) - No debug logging
# debug_log=1 (troubleshooting) - Output debug logs (viewable with DebugView)
debug_log=0

# Chrome Features to Disable
# Leave empty for defaults (recommended): WinSboxNoFakeGdiInit,WebUIInProcessResourceLoading
# Or customize with your own features
disable_features=

# Additional command-line arguments
command_line=

[dir_setting]
# Data directory (user settings, bookmarks, extensions, etc.)
data=%app%\..\Data

# Cache directory (temporary files)
# Performance tip: Place on SSD for best video streaming performance
cache=%app%\..\Cache

[hotkey]
# Boss Key - Quickly hide/show browser and mute/unmute audio
# Format: Ctrl+Alt+B or Win+H, etc.
# Leave empty to disable
boss_key=
```

#### Configuration Options

##### `[general]` Section

- **`win32k`** (default: `0`)
  - `0` - **Recommended** 🟢
    - Maintains Chrome's default sandbox security
    - Ensures GPU hardware acceleration works correctly
    - **Best video streaming performance** (Twitch, YouTube, etc.)
    - Low CPU usage (10-20%)
  - `1` - **Compatibility Mode** 🟡
    - Use only if browser crashes at startup
    - May fix conflicts with certain antivirus software (Symantec, McAfee)
    - ⚠️ May break hardware video decoding
    - ⚠️ Twitch/YouTube may play in lower quality
    - ⚠️ Increased CPU usage (50-80%)

- **`debug_log`** (default: `0`)
  - `0` - No debug logging, minimal performance impact
  - `1` - Output debug logs for troubleshooting
    - Outputs via OutputDebugString (viewable with [DebugView](https://learn.microsoft.com/en-us/sysinternals/downloads/debugview))
    - Logs command line arguments, errors, and portable mode operations
    - Use only when investigating issues

- **`command_line`** (default: empty)
  - Additional Chrome command-line flags
  - Example: `command_line=--force-dark-mode --enable-features=WebUIDarkMode`

- **`disable_features`** (default: `WinSboxNoFakeGdiInit,WebUIInProcessResourceLoading`)
  - Specifies which Chrome features to disable
  - **Leave empty or unspecified**: Use default compatibility features (recommended)
  - **Specify value**: Use your custom features (replaces defaults)
  - Common features:
    - `WinSboxNoFakeGdiInit` - Fix GPU initialization issues
    - `WebUIInProcessResourceLoading` - Improve WebUI compatibility
    - `RendererCodeIntegrity` - Allow DLL injection
    - `AutofillServerCommunication` - Disable autofill sync
    - `TranslateUI` - Disable translation prompts
  - Examples:
    ```ini
    # Use defaults (recommended)
    disable_features=

    # Custom features
    disable_features=RendererCodeIntegrity,TranslateUI

    # Add more features
    disable_features=WinSboxNoFakeGdiInit,WebUIInProcessResourceLoading,TranslateUI
    ```

##### `[dir_setting]` Section

- **`data`** (default: `%app%\..\Data`)
  - User data directory (profiles, bookmarks, extensions, etc.)
  - Supported formats:
    - Relative paths: `data=..\Data`
    - Absolute paths: `data=C:\MyBrowser\Data`
    - Environment variables: `data=%LOCALAPPDATA%\VivaldiPlus\Data`
    - `%app%` variable: `data=%app%\..\Data`

- **`cache`** (default: `%app%\..\Cache`)
  - Cache directory (temporary files, media cache, etc.)
  - **Performance tips**:
    - Place on SSD (not HDD)
    - Ensure at least 2GB free space
    - Avoid network drives

##### `[hotkey]` Section

- **`boss_key`** (default: empty, disabled)
  - Boss Key - Quickly hide/show browser and mute/unmute audio
  - **Features**:
    - Press hotkey to instantly hide all Vivaldi windows and mute all audio
    - Press again to restore windows and unmute audio
    - Global hotkey (system-wide)
  - **Format**: `Modifier+Modifier+Key`
    - Modifiers: `Ctrl` (or `Control`), `Alt`, `Shift`, `Win`
    - Keys: `A-Z`, `0-9`, `F1-F24`, arrow keys, `Esc`, `Tab`, `Space`, etc.
  - **Examples**:
    ```ini
    boss_key=Ctrl+Alt+B
    boss_key=Ctrl+Shift+H
    boss_key=Win+H
    boss_key=Ctrl+`
    ```
  - **Important notes**:
    - Make sure the hotkey doesn't conflict with other applications or system hotkeys
    - Leave empty to disable boss key functionality
    - Audio mute state is preserved when hiding (unmute only if originally not muted)

#### Troubleshooting

**Issue**: Twitch/YouTube videos load slowly or play in low quality (160p)
- **Check**: Open `chrome://gpu`, verify "Video Decode" shows "Hardware accelerated"
- **Solution**: Ensure `config.ini` has `win32k=0` (or no config.ini exists)

**Issue**: Browser crashes immediately at startup
- **Solution**: Create `config.ini`, set `win32k=1`
- **Recommendation**: Consider excluding browser from antivirus real-time scanning

**Issue**: Video stuttering or frequent buffering
- **Check**: Cache directory location and disk speed
- **Solution**: Move cache to SSD: `cache=C:\Path\To\SSD\Cache`

**Issue**: Boss key not working
- **Solution**:
  1. Make sure the hotkey doesn't conflict with system hotkeys
  2. Try a different key combination
  3. Set `debug_log=1` and use DebugView to see any error messages
  4. Make sure you're pressing the exact combination specified

**Issue**: Windows restore in wrong order or not all windows show
- **Explanation**: Boss key restores windows in reverse creation order
- **Solution**: Try hiding and showing again, or restart the browser

For detailed configuration documentation, see `config.ini.example` file.

### Architecture Support

Vivaldi Plus now supports multiple CPU architectures:

- **x86** (32-bit) - Legacy 32-bit Windows systems
- **x64** (64-bit) - Modern 64-bit Windows systems
- **ARM64** - Windows on ARM devices (Surface Pro X, Snapdragon laptops, etc.)

#### Why Microsoft Detours?

With Vivaldi browser releasing ARM64 builds to support Windows on ARM devices, the original MinHook library couldn't meet our needs because **MinHook doesn't support ARM64 architecture**.

To enable Vivaldi Plus on all architectures, we've migrated from MinHook to [Microsoft Detours](https://github.com/microsoft/Detours):

- ✅ **Complete architecture support** - Detours natively supports x86, x64, and ARM64
- ✅ **Official Microsoft maintenance** - Developed and maintained by Microsoft, stable and reliable
- ✅ **Mature ecosystem** - Widely used in various Windows platform tools
- ✅ **Better compatibility** - Perfect compatibility with Windows on ARM

Now you can enjoy Vivaldi Plus's portable experience on any Windows device architecture!

### Download & Installation

#### Get Vivaldi Browser
Visit https://vivaldi.czyt.tech for download links

#### Get Vivaldi Plus
- **Stable Builds**: Download from [Releases](https://github.com/ca-x/vivaldi_plus/releases) page
- **Nightly Builds**: [Powered by nightly.link](https://nightly.link/ca-x/vivaldi_plus/workflows/nightly/main) - Auto-built after each code push

> **Build Types:**
> - **Stable**: Tested official releases, recommended for most users
> - **Nightly**: Contains latest features and fixes, but may be unstable

#### Installation
Place `version.dll` in the Vivaldi portable directory (same directory as `vivaldi.exe`)

### Building

```bash
# Install xmake
# Clone repository (use --recursive to include Detours submodule)
git clone --recursive https://github.com/ca-x/vivaldi_plus.git
cd vivaldi_plus

# Build x64 (recommended)
xmake f -a x64 -m release
xmake

# Build ARM64 (Windows on ARM)
xmake f -a arm64 -m release
xmake

# Build x86 (32-bit)
xmake f -a x86 -m release
xmake
```

Compiled DLLs will be output to `build/release/<architecture>/version.dll`

### Original Project
Based on [chromePlus](https://github.com/icy37785/chrome_plus)

> Chrome 118+ fixes referenced from [Bush2021's project](https://github.com/Bush2021/chrome_plus). Special thanks.

---

## License
See [LICENSE](LICENSE) file for details.
