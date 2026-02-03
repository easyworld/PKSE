# PKSE 中文字体支持实现说明

## 问题描述

原始的`PKSEFramebuffer::drawText`方法只支持ASCII字符（32-127），使用自制的8x8点阵字库。当尝试渲染中文字符时，它们会被替换为"???"。

## 解决方案

实现了使用Nintendo Switch系统共享字体（plSharedFont服务）来渲染Unicode字符，包括中文、日文和其他多语言字符。

## 技术实现

### 1. pl服务初始化

```cpp
// 在PKSEFramebuffer构造函数中
Result rc = plInitialize(PlServiceType_User);
if (R_SUCCEEDED(rc)) {
    rc = plGetSharedFontByType(&standardFont, PlSharedFontType_Standard);
    if (R_SUCCEEDED(rc)) {
        plServiceInitialized = true;
    }
}
```

- PlSharedFontType_Standard包含：
  - 拉丁字母（Latin）
  - 日文假名（Hiragana, Katakana）
  - CJK统一表意文字（简体中文、繁体中文、日文汉字、韩文）

### 2. UTF-8到Unicode转换

```cpp
uint32_t utf8ToUnicode(const char*& text);
```

支持1-4字节的UTF-8序列：
- 1字节：ASCII (0x00-0x7F)
- 2字节：扩展拉丁字符 (0x80-0x7FF)
- 3字节：CJK字符等 (0x800-0xFFFF)
- 4字节：补充字符 (0x10000-0x10FFFF)

### 3. 字形渲染

```cpp
void drawGlyphFromSharedFont(int x, int y, uint32_t codepoint, Color color, int& glyphWidth);
```

流程：
1. 调用`plGetSharedFontGlyphInfo`获取字形度量信息
2. 调用`plGetSharedFontGlyphBitmap`获取字形位图数据
3. 使用alpha混合渲染字形到framebuffer
4. 返回字形宽度用于文本定位

### 4. 混合字体渲染策略

`drawText`方法实现智能字体选择：

| 字符类型 | 渲染方法 | 说明 |
|---------|---------|------|
| ASCII (32-127) | 8x8点阵字体 | 高性能，已有字形 |
| CJK字符 (E4-E9开头的UTF-8) | 系统共享字体 | 完整Unicode支持 |
| 特殊符号 (★♀♂) | 自定义8x8字形 | Pokemon专用符号 |
| 其他Unicode | 系统共享字体 | 回退方案 |

### 5. 字形度量处理

系统字体提供详细的字形度量：
- `bearingX`: 字形水平偏移
- `bearingY`: 字形垂直偏移（基线）
- `advance`: 光标前进距离
- `width`, `height`: 字形位图尺寸

正确使用这些度量确保文本对齐和间距正确。

## API变化

### 新增私有成员

```cpp
class PKSEFramebuffer {
private:
    bool plServiceInitialized;
    PlFontData standardFont;
    
    void drawTextWithSharedFont(int x, int y, const char* text, Color color, int& outWidth);
    uint32_t utf8ToUnicode(const char*& text);
    void drawGlyphFromSharedFont(int x, int y, uint32_t codepoint, Color color, int& glyphWidth);
};
```

### 公共API保持不变

```cpp
void drawText(int x, int y, const char* text, Color color);
void drawText(int x, int y, const std::string& text, Color color);
```

现有代码无需修改即可自动支持中文！

## 性能考虑

### 优化措施

1. **ASCII字符保持8x8字体**
   - 避免对常用英文字符调用pl服务
   - 保持原有渲染性能

2. **按需加载字形**
   - 只在遇到CJK字符时调用pl服务
   - 每次只获取单个字形数据

3. **智能检测**
   - 通过UTF-8首字节快速识别CJK字符
   - 避免不必要的字体查询

### 性能影响

- **ASCII文本**：无性能影响（继续使用8x8字体）
- **中文文本**：首次渲染略慢（需要从pl服务获取字形）
- **混合文本**：根据中文字符比例有轻微性能影响

## 兼容性

### 系统要求

- Nintendo Switch系统固件：任何支持plSharedFont服务的版本
- libnx版本：支持pl服务的任何版本

### 向后兼容

- 如果pl服务初始化失败，自动回退到原有行为
- ASCII字符渲染不受影响
- 不支持的字符显示为'?'

## 示例代码

### 基本使用

```cpp
PKSEFramebuffer fb;

// 纯英文 - 使用8x8字体
fb.drawText(100, 100, "Hello World", Colors::White);

// 纯中文 - 使用系统字体
fb.drawText(100, 120, "你好世界", Colors::White);

// 混合文本 - 自动切换
fb.drawText(100, 140, "Pokemon 宝可梦", Colors::Yellow);

fb.flush();
```

### UI示例

```cpp
// 用户选择界面
fb.drawText(20, 20, "选择用户档案", Colors::Text);

// 宝可梦详情
fb.drawText(100, 200, "种类: 皮卡丘", Colors::Text);
fb.drawText(100, 220, "性格: 勇敢", Colors::Text);
fb.drawText(100, 240, "特性: 静电", Colors::Text);

// 混合符号
fb.drawText(100, 260, "异色: ★", Colors::Red);  // 特殊符号仍使用自定义字形
```

## 调试

### 检查pl服务状态

```cpp
if (fb.plServiceInitialized) {
    // pl服务可用，中文将正确显示
} else {
    // pl服务不可用，中文将显示为'?'
}
```

### 常见问题

1. **中文显示为方块或'?'**
   - 检查pl服务是否初始化成功
   - 确认系统字体包含所需字符

2. **字体大小不一致**
   - 这是正常的 - 中文字符（~16px）比ASCII（8px）大
   - 考虑调整布局以适应混合字体大小

3. **渲染性能问题**
   - 如果大量中文文本渲染慢，考虑缓存常用字形
   - 当前实现优先考虑正确性而非性能

## 未来改进

可能的优化方向：

1. **字形缓存**：缓存最近使用的字形位图，避免重复从pl服务获取

2. **文本预渲染**：对固定文本（如UI标签）预渲染到纹理

3. **可配置字体大小**：允许调整中文字体大小以更好地匹配UI设计

4. **字体回退链**：支持多个字体源，提高字符覆盖率

5. **字距调整**：更精细的字符间距控制

## 结论

通过集成Switch的plSharedFont服务，PKSE现在完全支持中文UI。该实现：

- ✅ 支持完整的Unicode/CJK字符集
- ✅ 保持ASCII文本的高性能渲染
- ✅ 向后兼容，无需修改现有代码
- ✅ 使用系统字体，无需额外资源
- ✅ 支持alpha混合，字体边缘平滑

现在所有的中文翻译都能正确显示，为中文用户提供完整的本地化体验！
