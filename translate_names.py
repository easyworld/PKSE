#!/usr/bin/env python3
"""
Translation script for PKSE Names files using PKHeX data.
This script reads PKHeX English and Chinese translation files and updates
the C++ name files in PKSE with official Pokemon Chinese translations.
"""

import re
import sys
from pathlib import Path
from typing import Dict, List, Tuple

# Paths
PKHEX_DIR = Path("/tmp/PKHeX_repo/PKHeX.Core/Resources/text")
PKSE_DIR = Path("/home/runner/work/PKSE/PKSE/src/Names")

def load_translation_mapping(en_file: Path, zh_file: Path) -> Dict[str, str]:
    """
    Load translation mapping from PKHeX files.
    Returns a dictionary mapping English names to Chinese names.
    """
    mapping = {}
    
    with open(en_file, 'r', encoding='utf-8') as f_en:
        en_lines = [line.strip() for line in f_en.readlines()]
    
    with open(zh_file, 'r', encoding='utf-8') as f_zh:
        zh_lines = [line.strip() for line in f_zh.readlines()]
    
    # Ensure both files have the same number of lines
    if len(en_lines) != len(zh_lines):
        print(f"Warning: {en_file.name} has {len(en_lines)} lines, "
              f"{zh_file.name} has {len(zh_lines)} lines")
    
    # Create mapping
    for i, (en_name, zh_name) in enumerate(zip(en_lines, zh_lines)):
        if en_name and zh_name:  # Skip empty lines
            mapping[en_name] = zh_name
            
            # Add special case mappings for characters that differ in PKSE
            if "♀" in en_name:
                mapping[en_name.replace("♀", "F")] = zh_name
            if "♂" in en_name:
                mapping[en_name.replace("♂", "M")] = zh_name
            if "'" in en_name:
                mapping[en_name.replace("'", "")] = zh_name
            if "." in en_name:
                mapping[en_name.replace(".", "")] = zh_name
            if " " in en_name:
                mapping[en_name.replace(" ", "")] = zh_name
    
    return mapping

def translate_cpp_file(cpp_file: Path, mapping: Dict[str, str], 
                       keep_english: bool = False) -> None:
    """
    Translate a C++ names file using the provided mapping.
    
    Args:
        cpp_file: Path to the C++ file to translate
        mapping: Dictionary mapping English names to Chinese names
        keep_english: If True, keep English names that don't have translations
    """
    print(f"\nTranslating {cpp_file.name}...")
    
    # Read the file
    with open(cpp_file, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Find all quoted strings and replace them
    translated_count = 0
    untranslated = []
    
    def replace_quoted_string(match):
        nonlocal translated_count
        original = match.group(1)
        
        # Skip comments
        if match.group(0).strip().startswith('//'):
            return match.group(0)
        
        # Try to find translation
        if original in mapping:
            translated_count += 1
            return f'"{mapping[original]}"'
        elif original in ["None", "Unknown", "", "???", "Egg"]:
            # Keep certain special values unchanged
            return match.group(0)
        else:
            untranslated.append(original)
            return match.group(0)
    
    # Replace all quoted strings
    new_content = re.sub(r'"([^"]*)"', replace_quoted_string, content)
    
    # Write back to file
    with open(cpp_file, 'w', encoding='utf-8') as f:
        f.write(new_content)
    
    # Count total entries (approximate)
    total_entries = content.count('",')
    
    print(f"  Translated: {translated_count} entries")
    if untranslated:
        unique_untranslated = list(set(untranslated))
        print(f"  Untranslated unique entries: {len(unique_untranslated)}")
        if len(unique_untranslated) <= 20:
            for item in sorted(unique_untranslated)[:20]:
                print(f"    - {item}")

def main():
    """Main translation function."""
    print("PKSE Translation Script - Using PKHeX Data")
    print("=" * 60)
    
    # Check if PKHeX directory exists
    if not PKHEX_DIR.exists():
        print(f"Error: PKHeX directory not found at {PKHEX_DIR}")
        print("Please run the clone command first.")
        sys.exit(1)
    
    # Translation tasks
    tasks = [
        {
            'name': 'Species Names',
            'en_file': PKHEX_DIR / 'other/en/text_Species_en.txt',
            'zh_file': PKHEX_DIR / 'other/zh-Hans/text_Species_zh-Hans.txt',
            'cpp_file': PKSE_DIR / 'SpeciesNames.cpp',
        },
        {
            'name': 'Ability Names',
            'en_file': PKHEX_DIR / 'other/en/text_Abilities_en.txt',
            'zh_file': PKHEX_DIR / 'other/zh-Hans/text_Abilities_zh-Hans.txt',
            'cpp_file': PKSE_DIR / 'AbilityNames.cpp',
        },
        {
            'name': 'Item Names',
            'en_file': PKHEX_DIR / 'items/text_Items_en.txt',
            'zh_file': PKHEX_DIR / 'items/text_Items_zh-Hans.txt',
            'cpp_file': PKSE_DIR / 'ItemNames.cpp',
        },
        # FormNames.cpp has a different structure (switch statement)
        # and will be handled separately
    ]
    
    # Process each translation task
    for task in tasks:
        print(f"\n{'=' * 60}")
        print(f"Processing: {task['name']}")
        print(f"{'=' * 60}")
        
        # Check if files exist
        if not task['en_file'].exists():
            print(f"Warning: {task['en_file']} not found, skipping...")
            continue
        if not task['zh_file'].exists():
            print(f"Warning: {task['zh_file']} not found, skipping...")
            continue
        if not task['cpp_file'].exists():
            print(f"Warning: {task['cpp_file']} not found, skipping...")
            continue
        
        # Load translation mapping
        print(f"Loading translations from PKHeX...")
        mapping = load_translation_mapping(task['en_file'], task['zh_file'])
        print(f"  Loaded {len(mapping)} translations")
        
        # Translate the C++ file
        translate_cpp_file(task['cpp_file'], mapping, keep_english=False)
    
    print(f"\n{'=' * 60}")
    print("Translation complete!")
    print(f"{'=' * 60}")

if __name__ == "__main__":
    main()
