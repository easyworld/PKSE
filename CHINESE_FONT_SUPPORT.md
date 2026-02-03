# PKSE 中文字体支持实现说明

## 实现完成！

PKSE现在支持完整的Unicode/CJK字符显示，包括中文、日文和韩文。

## 技术方案

基于Nintendo Switch官方示例（switch-examples/graphics/shared_font），使用**FreeType库**解析和渲染系统字体。

### 实现方式

**1. FreeType集成**
- 使用FreeType 2库进行字体渲染
- 通过pl服务获取Switch系统字体数据
- 从内存加载字体（无需外部文件）

**2. 渲染流程**
```cpp
1. plGetSharedFontByType() → 获取系统字体数据
2. FT_New_Memory_Face() → 从内存创建字体face
3. FT_Set_Char_Size() → 设置字体大小（16pt）
4. decode_utf8() → 解析UTF-8字符串
5. FT_Get_Char_Index() → 获取字符的字形索引
6. FT_Load_Glyph() → 加载字形
7. FT_Render_Glyph() → 渲染字形为位图
8. drawGlyph() → 绘制到framebuffer（alpha混合）
```

## 功能特性

### ✅ 支持的字符

- **完整Unicode支持**: 所有UTF-8字符
- **CJK字符**: 简体中文、繁体中文、日文、韩文
- **拉丁字符**: 英文和各种欧洲语言
- **特殊符号**: Pokemon专用符号（★♀♂）仍使用自定义字形
- **标点符号**: 所有Unicode标点

### ✅ 渲染特性

- **抗锯齿**: FreeType自动提供平滑的字体边缘
- **Alpha混合**: 支持半透明文本
- **自动换行**: 文本超出宽度自动换行
- **换行支持**: 支持\n字符换行
- **像素级精确**: 正确的字形度量和位置

### ✅ 向后兼容

- 如果FreeType初始化失败，自动回退到8x8 ASCII字体
- 保留所有原有功能
- 无需修改现有代码

## 代码示例

### 基本使用

```cpp
PKSEFramebuffer fb;

// 英文文本
fb.drawText(100, 100, "Hello World", Colors::White);

// 中文文本
fb.drawText(100, 120, "你好世界", Colors::White);

// 混合文本
fb.drawText(100, 140, "Pokemon 宝可梦", Colors::Yellow);

// 日文假名和汉字
fb.drawText(100, 160, "種類: ピカチュウ", Colors::Text);

// 特殊符号（使用自定义字形）
fb.drawText(100, 180, "异色: ★", Colors::Red);

// 多行文本
fb.drawText(100, 200, "第一行\n第二行\n第三行", Colors::Text);

fb.flush();
```

### UI实际应用

```cpp
// 用户选择界面
fb.drawText(20, 20, "选择用户档案", Colors::Text);

// 宝可梦详情
fb.drawText(100, 200, "种类: 皮卡丘", Colors::Text);
fb.drawText(100, 220, "性格: 勇敢", Colors::Text);
fb.drawText(100, 240, "特性: 静电", Colors::Text);
fb.drawText(100, 260, "携带道具: 神奇糖果", Colors::Text);
fb.drawText(100, 280, "亲密度: 255", Colors::Text);

// 状态信息
fb.drawText(100, 320, "HP: 100/100", Colors::Text);
fb.drawText(100, 340, "攻击: 55", Colors::Text);
fb.drawText(100, 360, "防御: 40", Colors::Text);
```

## 技术细节

### 依赖项

- **libnx**: Switch homebrew库（pl服务）
- **FreeType 2**: 字体渲染库（通过switch-freetype包）

### 构建配置

**Makefile更新**:
```makefile
CFLAGS += `$(PREFIX)pkg-config --cflags freetype2`
LIBS := -lnx `$(PREFIX)pkg-config --libs freetype2` -lz -llz4
```

### 初始化过程

```cpp
PKSEFramebuffer::PKSEFramebuffer() {
    // 1. 初始化pl服务
    plInitialize(PlServiceType_User);
    
    // 2. 获取Standard字体（支持CJK）
    plGetSharedFontByType(&fontData, PlSharedFontType_Standard);
    
    // 3. 初始化FreeType
    FT_Init_FreeType(&ftLibrary);
    
    // 4. 从内存创建字体face
    FT_New_Memory_Face(ftLibrary, fontData.address, fontData.size, 0, &ftFace);
    
    // 5. 设置字体大小（16pt @ 96 DPI）
    FT_Set_Char_Size(ftFace, 0, 16*64, 96, 96);
}
```

### 渲染过程

```cpp
void PKSEFramebuffer::drawText(int x, int y, const char* text, Color color) {
    // 遍历UTF-8字符串
    while (i < str_size) {
        // 解码UTF-8字符
        decode_utf8(&codepoint, &text[i]);
        
        // 获取字形
        glyph_index = FT_Get_Char_Index(ftFace, codepoint);
        FT_Load_Glyph(ftFace, glyph_index, FT_LOAD_DEFAULT);
        FT_Render_Glyph(ftFace->glyph, FT_RENDER_MODE_NORMAL);
        
        // 绘制字形位图（alpha混合）
        drawGlyph(&slot->bitmap, x + slot->bitmap_left, y - slot->bitmap_top, color);
        
        // 前进到下一个字符位置
        x += slot->advance.x >> 6;
    }
}
```

### 字形度量

FreeType提供精确的字形度量信息：
- `bitmap_left`: 字形水平偏移
- `bitmap_top`: 字形垂直偏移（基线）
- `advance.x`: 光标水平前进距离
- `advance.y`: 光标垂直前进距离
- `metrics.height`: 行高

### Alpha混合

```cpp
// 优化的alpha混合算法
finalR = ((r * alpha) + (bgR * (255 - alpha)) + 255) >> 8;
finalG = ((g * alpha) + (bgG * (255 - alpha)) + 255) >> 8;
finalB = ((b * alpha) + (bgB * (255 - alpha)) + 255) >> 8;
```

使用位移代替除法，提高性能。

## 性能考虑

### 优化措施

1. **按需渲染**: 只在需要时加载和渲染字形
2. **内存字体**: 直接从内存使用系统字体，无I/O开销
3. **优化混合**: 使用位移运算代替除法
4. **合理字体大小**: 16pt在Switch屏幕上清晰且高效

### 性能特征

- **首次渲染**: 略慢（FreeType需要解析字体和渲染字形）
- **后续帧**: 性能稳定
- **内存使用**: 约1-2MB（FreeType + 字体数据）
- **CPU使用**: 中等（字形渲染有一定开销）

## 故障排除

### 如果中文仍显示为'?'

1. **检查FreeType初始化**
   - 确认switch-freetype包已安装
   - 检查pkg-config能找到freetype2

2. **检查编译**
   - 确认Makefile正确包含FreeType
   - 检查编译时无FreeType相关错误

3. **运行时检查**
   ```cpp
   if (!freetypeInitialized) {
       // FreeType未初始化，检查日志
   }
   ```

### 常见问题

**Q: 字体太小/太大？**
A: 修改`FT_Set_Char_Size()`的大小参数：
```cpp
FT_Set_Char_Size(ftFace, 0, 20*64, 96, 96);  // 20pt
```

**Q: 某些字符显示为方块？**
A: Standard字体不包含该字符，尝试使用其他字体类型：
```cpp
plGetSharedFontByType(&fontData, PlSharedFontType_ChineseSimplified);
```

**Q: 性能不佳？**
A: 考虑：
- 减小字体大小
- 预渲染静态文本
- 使用字形缓存

## 参考资料

- [官方示例代码](https://github.com/switchbrew/switch-examples/tree/master/graphics/shared_font)
- [libnx pl服务文档](https://github.com/switchbrew/libnx/blob/master/nx/include/switch/services/pl.h)
- [FreeType文档](https://www.freetype.org/freetype2/docs/documentation.html)
- [Switch Homebrew开发](https://switchbrew.org/)

## 更新日志

### 2024年2月3日
- ✅ 实现FreeType集成
- ✅ 完整Unicode/CJK支持
- ✅ Alpha混合渲染
- ✅ 自动换行支持
- ✅ 向后兼容（失败时回退）
- ✅ 基于官方示例实现
- ✅ 专业字体渲染质量

## 总结

PKSE现在完全支持中文UI！实现方式：
- ✅ 使用FreeType渲染系统字体
- ✅ 完整Unicode支持
- ✅ 高质量抗锯齿渲染
- ✅ 向后兼容
- ✅ 性能优化

所有已翻译的中文文本现在都能正确、美观地显示！🎉
