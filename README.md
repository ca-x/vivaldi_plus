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
本DLL提供有限的自定义选项，在DLL同目录新建 `config.ini` 文件即可配置。

**配置示例：**
```ini
[dir_setting]
data=%app%\..\Data      # 数据目录
cache=%app%\..\Cache    # 缓存目录
```
> 可以使用 `%app%` 表示DLL所在目录

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
This DLL provides limited customization options. Create a `config.ini` file in the same directory as the DLL.

**Example configuration:**
```ini
[dir_setting]
data=%app%\..\Data      # Data directory
cache=%app%\..\Cache    # Cache directory
```
> You can use `%app%` to represent the DLL directory

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
