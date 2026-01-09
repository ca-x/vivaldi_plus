# disable_features 行为验证

## 核心逻辑

### ✅ 留空 → 使用默认

**配置示例:**
```ini
[general]
disable_features=
```

**代码逻辑:**
```cpp
if (result > 0 && features_buffer[0] != L'\0')
{
    // 用户指定了非空值
}
else
{
    // 留空或未指定 → 使用默认
    disable_features_ = L"WinSboxNoFakeGdiInit,WebUIInProcessResourceLoading";
}
```

**最终 Chrome 参数:**
```
--disable-features=WinSboxNoFakeGdiInit,WebUIInProcessResourceLoading
```

---

### ✅ 非空 → 完全使用用户设置

**配置示例:**
```ini
[general]
disable_features=RendererCodeIntegrity,TranslateUI
```

**代码逻辑:**
```cpp
if (result > 0 && features_buffer[0] != L'\0')
{
    // 非空 → 完全使用用户设置
    disable_features_ = features_buffer;  // "RendererCodeIntegrity,TranslateUI"
}
```

**最终 Chrome 参数:**
```
--disable-features=RendererCodeIntegrity,TranslateUI
```

**注意:** 默认值 `WinSboxNoFakeGdiInit,WebUIInProcessResourceLoading` **不会被添加**！

---

## 测试用例

| 配置内容 | Config::GetDisableFeatures() 返回值 | Chrome 最终参数 |
|---------|-------------------------------------|-----------------|
| 不存在 config.ini | `WinSboxNoFakeGdiInit,WebUIInProcessResourceLoading` | `--disable-features=WinSboxNoFakeGdiInit,WebUIInProcessResourceLoading` |
| `disable_features=` | `WinSboxNoFakeGdiInit,WebUIInProcessResourceLoading` | `--disable-features=WinSboxNoFakeGdiInit,WebUIInProcessResourceLoading` |
| `; disable_features=` (注释) | `WinSboxNoFakeGdiInit,WebUIInProcessResourceLoading` | `--disable-features=WinSboxNoFakeGdiInit,WebUIInProcessResourceLoading` |
| `disable_features=Foo` | `Foo` | `--disable-features=Foo` |
| `disable_features=Foo,Bar` | `Foo,Bar` | `--disable-features=Foo,Bar` |
| `disable_features=WinSboxNoFakeGdiInit,WebUIInProcessResourceLoading,Baz` | `WinSboxNoFakeGdiInit,WebUIInProcessResourceLoading,Baz` | `--disable-features=WinSboxNoFakeGdiInit,WebUIInProcessResourceLoading,Baz` |

---

## 关键代码路径

### 1. 配置读取 (`src/config.h:46-62`)

```cpp
// 读取配置
DWORD result = GetPrivateProfileStringW(L"general", L"disable_features", L"", features_buffer, 4096, config_path_.c_str());

// 判断逻辑
if (result > 0 && features_buffer[0] != L'\0')
{
    // 非空 → 用户设置
    disable_features_ = features_buffer;
    has_custom_disable_features_ = true;
}
else
{
    // 留空 → 默认值
    disable_features_ = L"WinSboxNoFakeGdiInit,WebUIInProcessResourceLoading";
    has_custom_disable_features_ = false;
}
```

### 2. 参数合并 (`src/portable.h:160-171`)

```cpp
// 获取要添加的特性（要么是用户设置，要么是默认值）
std::wstring features_to_add = Config::Instance().GetDisableFeatures();

// 合并到命令行参数
if (!features_to_add.empty())
{
    if (!combined_features.empty())
    {
        combined_features += L",";
    }
    combined_features += features_to_add;  // 直接使用，不再额外添加默认值
}
```

---

## 用户文档说明

### config.ini.example 中的说明

```ini
; BEHAVIOR:
;   - If NOT specified or empty: Uses default compatibility features
;     Default: WinSboxNoFakeGdiInit,WebUIInProcessResourceLoading
;   - If specified: Uses ONLY your specified features (replaces defaults)
```

### README.md 中的说明

**中文:**
```markdown
- **`disable_features`** (默认: `WinSboxNoFakeGdiInit,WebUIInProcessResourceLoading`)
  - 指定要禁用的 Chrome 特性
  - **留空或不指定**: 使用默认兼容性特性 (推荐)
  - **指定值**: 使用您的自定义特性 (替换默认值)
```

**英文:**
```markdown
- **`disable_features`** (default: `WinSboxNoFakeGdiInit,WebUIInProcessResourceLoading`)
  - Specifies which Chrome features to disable
  - **Leave empty or unspecified**: Use default compatibility features (recommended)
  - **Specify value**: Use your custom features (replaces defaults)
```

---

## 常见误解澄清

### ❌ 误解 1: "指定值会添加到默认值"

**错误理解:**
```ini
disable_features=TranslateUI
```
→ 以为会变成: `WinSboxNoFakeGdiInit,WebUIInProcessResourceLoading,TranslateUI`

**实际行为:**
→ 只使用: `TranslateUI`

**正确做法 (如果想要添加):**
```ini
disable_features=WinSboxNoFakeGdiInit,WebUIInProcessResourceLoading,TranslateUI
```

---

### ❌ 误解 2: "留空会禁用所有特性"

**错误理解:**
```ini
disable_features=
```
→ 以为不禁用任何特性

**实际行为:**
→ 使用默认值: `WinSboxNoFakeGdiInit,WebUIInProcessResourceLoading`

**如果真想不禁用任何特性 (不推荐):**
可能需要修改代码，或者使用一个无效的特性名如 `None`

---

## 实现优势

1. **✅ 简洁明了**: 留空=默认，非空=自定义
2. **✅ 完全控制**: 用户可以完全决定禁用哪些特性
3. **✅ 安全默认**: 不修改配置时自动使用最佳实践
4. **✅ 向后兼容**: 现有用户（无 config.ini）自动使用默认值
5. **✅ 符合预期**: 与大多数配置系统的行为一致

---

## 验证方法

### 方法 1: 查看 chrome://version

1. 启动浏览器
2. 打开 `chrome://version`
3. 查看 "命令行" / "Command Line" 部分
4. 查找 `--disable-features=...` 参数
5. 确认特性列表符合预期

### 方法 2: 添加调试日志

在 `config.h` 的 `LoadConfig()` 中添加:
```cpp
OutputDebugStringW((L"disable_features: " + disable_features_).c_str());
```

使用 DebugView 工具查看输出。

---

**验证日期**: 2026-01-09
**实现版本**: vivaldi_plus v2.1.0
**状态**: ✅ 完全符合需求
