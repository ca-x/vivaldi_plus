# Vivaldi Plus

[![build status](https://github.com/czyt/vivaldi_plus/actions/workflows/build.yml/badge.svg)](https://github.com/czyt/vivaldi_plus/actions/workflows/build.yml)

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

本DLL提供配置选项，在DLL同目录新建 `config.ini` 文件即可配置。

#### 快速开始
1. 复制 `config.ini.example.zh-CN` 为 `config.ini`
2. 根据需要修改配置项（通常使用默认值即可）
3. 重启浏览器

#### 配置文件说明

**完整配置示例：**
```ini
[general]
# Win32k 系统调用策略
# win32k=0 (推荐) - 保持默认沙箱，确保 GPU 硬件加速和视频流性能
# win32k=1 (兼容) - 仅当 Chrome 启动崩溃时使用，可能影响视频播放质量
win32k=0

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

详细配置说明请查看 `config.ini.example.zh-CN` 文件。

### 下载与安装

#### 获取Vivaldi浏览器
请访问 https://vivaldi.czyt.tech 获取下载链接

#### 获取Vivaldi Plus
- **Release版本**：从 [Releases](https://github.com/ca-x/vivaldi_plus/releases) 页面下载
- **开发版本**：[Powered by nightly.link](https://nightly.link/ca-x/vivaldi_plus/workflows/build/main)

#### 安装方法
将 `version.dll` 放入解压版Vivaldi目录（`vivaldi.exe` 同目录）即可

### 构建

```bash
# 安装xmake
# 克隆仓库
git clone --recursive https://github.com/ca-x/vivaldi_plus.git
cd vivaldi_plus

# 构建 x64
xmake f -a x64
xmake

# 构建 x86
xmake f -a x86
xmake
```

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

This DLL provides configuration options. Create a `config.ini` file in the same directory as the DLL.

#### Quick Start
1. Copy `config.ini.example` to `config.ini`
2. Modify configuration items as needed (defaults work for most cases)
3. Restart browser

#### Configuration File Format

**Complete configuration example:**
```ini
[general]
# Win32k System Call Policy
# win32k=0 (recommended) - Keep default sandbox, ensure GPU acceleration and video streaming
# win32k=1 (compatibility) - Use only if Chrome crashes at startup, may affect video quality
win32k=0

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

For detailed configuration documentation, see `config.ini.example` file.

### Download & Installation

#### Get Vivaldi Browser
Visit https://vivaldi.czyt.tech for download links

#### Get Vivaldi Plus
- **Release builds**: Download from [Releases](https://github.com/ca-x/vivaldi_plus/releases) page
- **Development builds**: [Powered by nightly.link](https://nightly.link/ca-x/vivaldi_plus/workflows/build/main)

#### Installation
Place `version.dll` in the Vivaldi portable directory (same directory as `vivaldi.exe`)

### Building

```bash
# Install xmake
# Clone repository
git clone --recursive https://github.com/ca-x/vivaldi_plus.git
cd vivaldi_plus

# Build x64
xmake f -a x64
xmake

# Build x86
xmake f -a x86
xmake
```

### Original Project
Based on [chromePlus](https://github.com/icy37785/chrome_plus)

> Chrome 118+ fixes referenced from [Bush2021's project](https://github.com/Bush2021/chrome_plus). Special thanks.

---

## License
See [LICENSE](LICENSE) file for details.
