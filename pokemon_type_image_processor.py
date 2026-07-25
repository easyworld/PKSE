from PIL import Image, ImageDraw, ImageFont
from collections import Counter
import os
import glob
import re

import requests

# ---------------------- 新增字体下载函数 ----------------------
def download_noto_font():
    """
    自动下载思源黑体粗体(NotoSansCJK-Bold.ttc)到本地fonts/目录
    返回：字体文件的本地绝对路径 / None（下载失败）
    """
    # 本地字体缓存目录（当前目录下fonts/）
    font_dir = "fonts"
    font_file = "NotoSansCJK-Bold.ttc"
    local_font_path = os.path.join(font_dir, font_file)
    
    # 1. 检查本地是否已有字体，有则直接返回路径
    if os.path.exists(local_font_path) and os.path.isfile(local_font_path):
        print(f"✅ 本地找到思源黑体粗体 → {local_font_path}")
        return local_font_path
    
    # 2. 无字体则创建fonts/目录
    if not os.path.exists(font_dir):
        os.makedirs(font_dir, exist_ok=True)
        print(f"📁 创建字体缓存目录 → {font_dir}")
    
    # 3. 思源黑体粗体 可靠下载源（GitHub开源镜像，永久有效）
    # 备用源：https://cdn.jsdelivr.net/npm/noto-sans-cjk-sc@latest/fonts/NotoSansCJKsc-Bold.otf
    FONT_DOWNLOAD_URL = "https://raw.githubusercontent.com/notofonts/noto-cjk/main/Sans/OTF/SimplifiedChinese/NotoSansCJKsc-Bold.otf"
    
    try:
        print(f"📥 正在下载思源黑体粗体（≈16MB）→ {FONT_DOWNLOAD_URL}")
        # 发起下载请求（超时30秒，分块下载避免内存溢出）
        response = requests.get(FONT_DOWNLOAD_URL, stream=True, timeout=30)
        response.raise_for_status()  # 捕获404/500等下载错误
        
        # 分块保存字体文件到本地
        with open(local_font_path, "wb") as f:
            for chunk in response.iter_content(chunk_size=8192):
                f.write(chunk)
        
        # 验证文件是否保存成功
        if os.path.getsize(local_font_path) < 1024 * 1024:  # 小于1MB则为下载失败
            os.remove(local_font_path)
            print("❌ 字体下载失败：文件大小异常（可能是无效链接）")
            return None
        
        print(f"✅ 思源黑体粗体下载完成 → {local_font_path}")
        return local_font_path
    
    except requests.exceptions.RequestException as e:
        print(f"❌ 字体下载失败：网络错误 → {e}")
        return None
    except Exception as e:
        print(f"❌ 字体保存失败 → {e}")
        if os.path.exists(local_font_path):
            os.remove(local_font_path)
        return None
# ---------------------- 字体下载函数结束 ----------------------

# 类型名称映射表
TYPE_NAMES = {
    0: "一般",      # Normal
    1: "格斗",      # Fighting
    2: "飞行",      # Flying
    3: "毒",        # Poison
    4: "地面",      # Ground
    5: "岩石",      # Rock
    6: "虫",        # Bug
    7: "幽灵",      # Ghost
    8: "钢",        # Steel
    9: "火",        # Fire
    10: "水",       # Water
    11: "草",       # Grass
    12: "电",       # Electric
    13: "超能力",   # Psychic
    14: "冰",       # Ice
    15: "龙",       # Dragon
    16: "恶",       # Dark
    17: "妖精"      # Fairy
}

def get_type_number_from_filename(filename):
    """
    从文件名中提取数字
    
    参数:
        filename: 文件名（如 "0.png" 或 "type_5.png"）
    
    返回:
        数字或None
    """
    # 移除扩展名
    name_without_ext = os.path.splitext(filename)[0]
    
    # 尝试匹配数字
    match = re.search(r'\d+', name_without_ext)
    if match:
        return int(match.group())
    return None

def process_image(image_path):
    """
    处理图片：填充最多的颜色并添加文字
    
    参数:
        image_path: PNG文件路径
    """
    try:
        # 从文件名获取类型编号
        filename = os.path.basename(image_path)
        type_number = get_type_number_from_filename(filename)
        
        if type_number is None:
            print(f"跳过 {filename}：无法从文件名中提取数字")
            return None
        
        if type_number not in TYPE_NAMES:
            print(f"跳过 {filename}：数字 {type_number} 不在有效范围内 (0-17)")
            return None
        
        text = TYPE_NAMES[type_number]
        
        # 读取图片
        img = Image.open(image_path)
        
        # 确保图片是200x40
        if img.size != (200, 40):
            print(f"跳过 {filename}：图片尺寸为 {img.size}，不是 200x40")
            return None
        
        # 保持原始模式，支持RGBA
        original_mode = img.mode
        
        # 统计所有非透明像素的颜色，找出最多的颜色
        if img.mode == 'RGBA':
            # 对于RGBA图片，只统计不透明的像素
            pixels = list(img.getdata())
            # 过滤掉完全透明的像素 (alpha = 0)
            opaque_pixels = [p[:3] for p in pixels if p[3] > 128]  # alpha > 128 视为不透明
            
            if not opaque_pixels:
                print(f"跳过 {filename}：图片没有不透明的像素")
                return None
                
            color_counter = Counter(opaque_pixels)
            most_common_color = color_counter.most_common(1)[0][0]
        else:
            # 对于RGB或其他模式
            if img.mode != 'RGB':
                img = img.convert('RGB')
            pixels = list(img.getdata())
            color_counter = Counter(pixels)
            most_common_color = color_counter.most_common(1)[0][0]
        
        # 创建绘图对象
        draw = ImageDraw.Draw(img)
        
        # 填充矩形区域 (50,0) 到 (180,40)
        # 如果是RGBA模式，需要添加alpha值
        if img.mode == 'RGBA':
            fill_color = most_common_color + (255,)  # 添加完全不透明的alpha值
        else:
            fill_color = most_common_color
            
        draw.rectangle([(50, 0), (180, 40)], fill=fill_color)
        
        # 加载Windows默认中文字体
        target_font_size = 36
        # 先尝试下载/获取本地思源黑体粗体路径
        local_noto_font = download_noto_font()
        
        try:
            # 构建字体路径列表
            font_paths = []
            # 如果本地有下载的思源粗体，优先加入
            if local_noto_font:
                font_paths.append(local_noto_font)
            
            font = None
            for font_path in font_paths:
                try:
                    font = ImageFont.truetype(font_path, target_font_size)
                    break
                except:
                    continue
            
            if font is None:
                font = ImageFont.load_default()
        except Exception as e:
            font = ImageFont.load_default()
        
        # 使用getbbox获取实际文字边界
        left, top, right, bottom = font.getbbox(text)
        text_height = bottom - top
        
        # 目标中心点
        center_x = 120
        center_y = 20
        
        # 计算文字位置
        # 水平方向：center_x应该在文字的正中间
        # 文字实际内容从left开始，宽度为text_width
        # 所以文字中心相对于绘制原点的位置是 left + text_width/2
        # 要让这个中心在center_x，绘制原点应该在 center_x - (left + text_width/2)
        text_x = center_x - (left + right) / 2
        text_y = center_y - top - text_height / 2
        
        # 绘制白色文字
        if img.mode == 'RGBA':
            draw.text((text_x, text_y), text, fill=(255, 255, 255, 255), font=font)
        else:
            draw.text((text_x, text_y), text, fill=(255, 255, 255), font=font)
        
        # 覆盖原文件
        img.save(image_path)
        print(f"✓ {filename} -> '{text}' (颜色: {most_common_color}, bbox: L{left} R{right} T{top} B{bottom}, 位置: {text_x:.1f}, {text_y:.1f})")
        return True
        
    except Exception as e:
        print(f"✗ 处理失败: {filename} - 错误: {e}")
        return False


def process_all_png_files():
    """
    处理当前目录下romfs/sprites/types文件夹里所有PNG文件
    """
    # 定义目标文件夹路径（当前目录 -> romfs -> sprites -> types）
    target_dir = "romfs/sprites/types"
    # 检查文件夹是否存在，不存在则提示并返回
    if not os.path.exists(target_dir):
        print(f"错误：目标文件夹不存在 -> {os.path.abspath(target_dir)}")
        return False
    if not os.path.isdir(target_dir):
        print(f"错误：指定路径不是文件夹 -> {os.path.abspath(target_dir)}")
        return False
    
    # 获取目标文件夹下所有PNG文件（兼容Windows/Linux路径）
    png_files = glob.glob(os.path.join(target_dir, "*.png"))
    
    if not png_files:
        print("当前目录下没有找到PNG文件")
        return False

    expected_files = [os.path.join(target_dir, f"{type_number}.png") for type_number in TYPE_NAMES]
    missing_files = [os.path.basename(path) for path in expected_files if not os.path.isfile(path)]
    if missing_files:
        print(f"错误：缺少属性图片 -> {', '.join(missing_files)}")
        return False
    
    print(f"找到 {len(png_files)} 个PNG文件\n")
    
    success_count = 0
    skip_count = 0
    fail_count = 0
    
    # CI 使用的标准文件名为 0.png 到 17.png。
    png_files = expected_files
    
    for png_file in png_files:
        result = process_image(png_file)
        if result is True:
            success_count += 1
        elif result is False:
            fail_count += 1
        else:
            skip_count += 1
    
    print(f"\n{'=' * 50}")
    print(f"处理完成!")
    print(f"成功: {success_count} 个")
    print(f"失败: {fail_count} 个")
    print(f"跳过: {skip_count} 个")
    print(f"{'=' * 50}")
    return success_count == len(TYPE_NAMES) and fail_count == 0 and skip_count == 0


# 主程序
if __name__ == "__main__":
    print("=" * 50)
    print("宝可梦属性图片批量处理工具")
    print("=" * 50)
    print("支持的文件名格式: 0.png, 1.png, ..., 17.png")
    print("=" * 50 + "\n")
    
    if not process_all_png_files():
        raise SystemExit(1)
