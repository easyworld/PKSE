#!/usr/bin/env python3
"""
Script to translate FormNames.cpp using PKHeX data.
FormNames.cpp has a different structure (switch/case) so needs special handling.
"""

from pathlib import Path

# Load PKHeX form translations
PKHEX_DIR = Path("/tmp/PKHeX_repo/PKHeX.Core/Resources/text")
en_file = PKHEX_DIR / "other/en/text_Forms_en.txt"
zh_file = PKHEX_DIR / "other/zh-Hans/text_Forms_zh-Hans.txt"

# Load translations
with open(en_file, 'r', encoding='utf-8') as f:
    en_forms = [line.strip() for line in f.readlines()]

with open(zh_file, 'r', encoding='utf-8') as f:
    zh_forms = [line.strip() for line in f.readlines()]

# Create mapping
form_mapping = {}
for en, zh in zip(en_forms, zh_forms):
    if en and zh:
        form_mapping[en] = zh
        # Handle variations
        if " " in en:
            form_mapping[en.replace(" ", "")] = zh

# Add manual mappings for specific regional forms
regional_mappings = {
    "Alolan": "阿罗拉的样子",
    "Galarian": "伽勒尔的样子",
    "Hisuian": "洗翠的样子", 
    "Paldean": "帕底亚的样子",
}

form_mapping.update(regional_mappings)

# Read FormNames.cpp
form_file = Path("/home/runner/work/PKSE/PKSE/src/Names/FormNames.cpp")
with open(form_file, 'r', encoding='utf-8') as f:
    content = f.read()

# Replace form name strings
import re

def replace_form(match):
    original = match.group(1)
    if original in form_mapping:
        return f'return "{form_mapping[original]}"'
    return match.group(0)

# Replace return statements with quoted strings
new_content = re.sub(r'return "([^"]+)"', replace_form, content)

# Write back
with open(form_file, 'w', encoding='utf-8') as f:
    f.write(new_content)

print("FormNames.cpp translated successfully!")
print(f"Applied {len(form_mapping)} form translations")
