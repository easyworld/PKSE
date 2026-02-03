# PKSE 中文字体支持实现说明（当前限制）

## 当前状态

由于libnx的pl服务API限制，**当前版本暂不支持中文字符显示**。

### 原因说明

**技术限制**:
- libnx的pl服务只提供原始字体数据访问（`plGetSharedFontByType`）
- 不提供字形渲染API（如`plGetSharedFontGlyphInfo`、`plGetSharedFontGlyphBitmap`等）
- 要渲染TrueType/OpenType字体需要额外的字体解析和光栅化库

**尝试的方案**:
原始实现尝试使用假定的pl字形渲染API，但这些API在libnx中不存在。

### 当前功能

**✅ 支持的字符**:
- ASCII字符（32-127）：使用8x8点阵字体
- 特殊Pokemon符号（★♀♂）：使用自定义8x8字形
- Unicode引号和破折号：自动映射到ASCII等效字符
- 拉丁扩展字符（À, É, Ñ等）：映射到基本字母

**❌ 不支持的字符**:
- CJK字符（中文、日文、韩文）：显示为'?'
- 其他多字节Unicode字符：显示为'?'

### 代码实现

```cpp
// UTF-8检测和处理
if ((c >= 0xE4 && c <= 0xE9) || c == 0xE3) {
    // CJK字符 - 跳过3字节并显示占位符
    if (text[1] && text[2]) {
        bytesToSkip = 3;
        c = '?';  // 使用'?'作为占位符
    }
}
```

### 翻译数据

虽然不能显示中文字符，但项目已完成：
- ✅ 所有UI字符串翻译为中文
- ✅ 宝可梦、道具、特性等名称翻译
- ✅ 使用PKHeX官方翻译数据

这些翻译为未来支持中文显示做好了准备。

## 未来改进方案

要实现完整的中文显示支持，有以下几种方案：

### 方案1: 集成FreeType库（推荐）

**优点**:
- 完整的TrueType/OpenType字体支持
- 专业的字形渲染质量
- 支持所有Unicode字符

**实施步骤**:
1. 添加FreeType到Makefile依赖
   ```makefile
   LIBS := -lnx -lfreetype -lz -llz4
   ```

2. 初始化FreeType并加载系统字体
   ```cpp
   FT_Library library;
   FT_Face face;
   FT_Init_FreeType(&library);
   
   // 使用pl服务获取字体数据
   PlFontData fontData;
   plGetSharedFontByType(&fontData, PlSharedFontType_Standard);
   
   // 从内存加载字体
   FT_New_Memory_Face(library, (FT_Byte*)fontData.address, 
                      fontData.size, 0, &face);
   ```

3. 实现字形渲染
   ```cpp
   FT_Set_Pixel_Sizes(face, 0, 16);  // 设置字体大小
   FT_Load_Char(face, codepoint, FT_LOAD_RENDER);
   // 渲染face->glyph->bitmap
   ```

**工作量**: 中等（约2-3天）

### 方案2: 预渲染位图字体

**优点**:
- 无需外部库依赖
- 渲染速度快
- 实现简单

**缺点**:
- 文件体积大（常用3000汉字约1-2MB）
- 不支持字体缩放
- 字体质量固定

**实施步骤**:
1. 使用工具生成常用汉字的位图数据
2. 将位图数据嵌入到romfs
3. 实现简单的位图查找和渲染

**工作量**: 低（约1天）

### 方案3: BDF/PCF字体解析器

**优点**:
- 简单的位图字体格式
- 容易解析
- 文件较小

**缺点**:
- 不支持抗锯齿
- 字体质量一般

**工作量**: 中等（约1-2天）

## 建议

**短期**（当前）:
- 保持当前实现（中文显示为'?'）
- 确保翻译数据完整
- 文档说明限制

**中期**（推荐）:
- 集成FreeType库
- 实现完整的中文字体渲染
- 提供高质量的显示效果

**长期**:
- 支持字体大小调整
- 支持多种字体样式
- 优化渲染性能

## 相关文件

- `src/UI/PKSEFramebuffer.cpp` - 文本渲染实现
- `src/UI/PKSEFramebuffer.h` - 接口定义
- `src/Names/*.cpp` - 已翻译的游戏数据
- `TRANSLATION_SUMMARY.md` - 翻译总结

## 参考资料

- [libnx pl service文档](https://github.com/switchbrew/libnx/blob/master/nx/include/switch/services/pl.h)
- [FreeType文档](https://www.freetype.org/freetype2/docs/documentation.html)
- [Switch Homebrew开发指南](https://switchbrew.org/)

## 更新日志

### 2024年2月3日
- ❌ 移除了无效的pl字形渲染API调用
- ✅ 修复编译错误
- ✅ 保留UTF-8解析基础设施
- ✅ 文档说明当前限制和改进方案
