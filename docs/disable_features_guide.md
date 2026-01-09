# disable_features 配置说明

## 概述

新增的 `disable_features` 配置项允许用户完全控制通过 `--disable-features` 标志禁用哪些 Chrome 特性。

---

## 工作原理

### 默认行为（推荐）

**不指定或留空 `disable_features`**:
```ini
[general]
disable_features=
```

或者完全省略这一行。

**效果**: 自动使用默认兼容性特性：
- `WinSboxNoFakeGdiInit` - 修复某些显卡驱动的 GPU 初始化问题
- `WebUIInProcessResourceLoading` - 改善 WebUI 加载兼容性

这些特性是从 chrome_plus 项目借鉴的最佳实践，确保最佳的视频流性能和兼容性。

---

### 自定义行为

**指定自定义特性**:
```ini
[general]
disable_features=RendererCodeIntegrity,TranslateUI
```

**效果**:
- ✅ 使用您指定的特性
- ❌ **不使用** 默认特性（完全替换）
- 您需要负责确保兼容性

---

## 配置优先级

特性标志的合并优先级（从高到低）：

1. **命令行 `--disable-features` 参数** (如果用户手动指定)
2. **config.ini 中的 `disable_features`** (用户配置)
3. **代码默认值** (`WinSboxNoFakeGdiInit,WebUIInProcessResourceLoading`)

所有来源的特性会被**智能合并**为单一的 `--disable-features` 标志。

### 示例

**场景 1**: 使用默认值
```ini
[general]
; 不指定 disable_features
```

**命令行启动**: `vivaldi.exe`

**最终结果**:
```
--disable-features=WinSboxNoFakeGdiInit,WebUIInProcessResourceLoading
```

---

**场景 2**: 自定义特性
```ini
[general]
disable_features=RendererCodeIntegrity,AutofillServerCommunication
```

**命令行启动**: `vivaldi.exe`

**最终结果**:
```
--disable-features=RendererCodeIntegrity,AutofillServerCommunication
```

注意：默认特性被完全替换！

---

**场景 3**: 混合命令行和配置
```ini
[general]
disable_features=TranslateUI
```

**命令行启动**: `vivaldi.exe --disable-features=Foo`

**最终结果**:
```
--disable-features=Foo,TranslateUI
```

所有来源的特性被合并，去重。

---

## 常用特性说明

### 默认特性（推荐保留）

#### `WinSboxNoFakeGdiInit`
- **作用**: 禁用沙箱中的假 GDI 初始化
- **为什么需要**: 某些显卡驱动需要真实的 GDI 路径才能正确初始化 GPU
- **影响**: 提高 GPU 加速兼容性，改善视频播放性能
- **风险**: 低，chrome_plus 项目验证过

#### `WebUIInProcessResourceLoading`
- **作用**: 禁用 WebUI 的进程内资源加载
- **为什么需要**: 改善内部页面（设置、历史等）的加载兼容性
- **影响**: 解决某些 WebUI 页面加载失败的问题
- **风险**: 低，主要影响内部页面

---

### 可选特性（按需添加）

#### `RendererCodeIntegrity`
- **作用**: 禁用渲染器代码完整性检查
- **何时需要**: 需要注入第三方 DLL 时
- **影响**: 允许非签名代码在渲染进程中运行
- **风险**: 中，降低安全性，但便携化通常需要

#### `AutofillServerCommunication`
- **作用**: 禁用自动填充服务器通信
- **何时需要**: 不想同步密码和表单数据
- **影响**: 自动填充仍可本地工作，但不会云同步
- **风险**: 低，仅影响同步功能

#### `TranslateUI`
- **作用**: 禁用翻译提示栏
- **何时需要**: 不需要页面翻译功能
- **影响**: 不会再提示翻译外语页面
- **风险**: 无，纯 UI 特性

#### `MediaRouter`
- **作用**: 禁用媒体路由（Cast）
- **何时需要**: 不使用 Chromecast 投屏
- **影响**: 移除投屏相关 UI 和功能
- **风险**: 低，仅影响投屏

---

## 推荐配置场景

### 场景 A: 普通用户（推荐）
```ini
[general]
win32k=0
disable_features=
```
**说明**: 使用所有默认值，最佳兼容性和性能

---

### 场景 B: 需要 DLL 注入
```ini
[general]
win32k=0
disable_features=WinSboxNoFakeGdiInit,WebUIInProcessResourceLoading,RendererCodeIntegrity
```
**说明**: 在默认特性基础上添加 `RendererCodeIntegrity`

---

### 场景 C: 最小化云服务
```ini
[general]
win32k=0
disable_features=WinSboxNoFakeGdiInit,WebUIInProcessResourceLoading,AutofillServerCommunication,TranslateUI
```
**说明**: 禁用同步和翻译，保持本地功能

---

### 场景 D: 兼容模式（启动崩溃）
```ini
[general]
win32k=1
disable_features=
```
**说明**: 启用 win32k，使用默认禁用特性
**警告**: 可能影响视频性能，仅在必要时使用

---

## 故障排除

### 问题 1: 不确定应该禁用哪些特性

**建议**: 留空，使用默认值
```ini
disable_features=
```

默认值经过测试，适用于大多数场景。

---

### 问题 2: 想在默认基础上添加特性

**错误做法**:
```ini
disable_features=TranslateUI
```
这会**替换**默认值，而不是添加！

**正确做法**:
```ini
disable_features=WinSboxNoFakeGdiInit,WebUIInProcessResourceLoading,TranslateUI
```
显式包含默认特性 + 您的自定义特性。

---

### 问题 3: 命令行参数被忽略

**症状**: 启动时指定了 `--disable-features=Foo`，但不生效

**原因**: 特性会被合并，不会被忽略

**验证**:
1. 打开 `chrome://version`
2. 查看 "Command Line" 部分
3. 应该看到合并后的完整标志

---

### 问题 4: 不知道某个特性的作用

**查询方法**:
1. 搜索 Chromium 源码: https://source.chromium.org/
2. 搜索特性名称，如 "WinSboxNoFakeGdiInit"
3. 查看相关代码和注释

**或者**:
- 提交 Issue 询问
- 参考 chrome_plus 项目的配置

---

## 高级技巧

### 技巧 1: 测试特性影响

创建两个配置文件:

**config.ini.default** (默认):
```ini
[general]
disable_features=
```

**config.ini.custom** (自定义):
```ini
[general]
disable_features=YourCustomFeature
```

启动前切换配置文件，对比效果。

---

### 技巧 2: 查看生效的特性

启动后检查:
```
chrome://version
```

在 "Command Line" 中找到 `--disable-features=...`，查看实际使用的特性列表。

---

### 技巧 3: 动态测试

不重启浏览器测试特性:
1. 新建快捷方式
2. 添加测试参数: `--disable-features=TestFeature`
3. 启动快捷方式
4. 对比两个实例的行为

---

## 与 chrome_plus 的差异

### chrome_plus 实现
```cpp
// chrome_plus 硬编码默认特性
combined_features.append(
    L"WinSboxNoFakeGdiInit,WebUIInProcessResourceLoading");
```
**特点**:
- ✅ 简单
- ❌ 不可配置

### vivaldi_plus 实现（本项目）
```cpp
// vivaldi_plus 从配置文件读取
std::wstring features_to_add = Config::Instance().GetDisableFeatures();
```
**特点**:
- ✅ 可配置
- ✅ 向后兼容（默认行为与 chrome_plus 相同）
- ✅ 灵活（用户可完全控制）

---

## 最佳实践

1. **默认优先**: 除非有明确需求，否则使用默认配置
2. **测试验证**: 修改配置后，验证 `chrome://gpu` 和 `chrome://version`
3. **记录原因**: 如果自定义特性，注释说明原因
4. **小步修改**: 一次只改一个特性，便于排查问题
5. **性能监控**: 注意视频播放性能和 CPU 占用

---

## 参考资料

- **Chromium 命令行标志**: https://peter.sh/experiments/chromium-command-line-switches/
- **chrome_plus 项目**: https://github.com/your-reference/chrome_plus
- **Chromium 源码搜索**: https://source.chromium.org/

---

**最后更新**: 2026-01-09
**适用版本**: vivaldi_plus v2.1.0+
