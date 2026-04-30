#!/usr/bin/env python3
import re
import shutil
from pathlib import Path

root = Path(__file__).parent
src = root / 'src'

mapping = {}

def pascal_case(name: str) -> str:
    parts = re.split('[^A-Za-z0-9]+', name)
    parts = [p for p in parts if p]
    return ''.join(p[0].upper() + p[1:] if len(p)>1 else p.upper() for p in parts)

# Collect files
files = list(src.rglob('*.cpp')) + list(src.rglob('*.hpp')) + list(src.rglob('*.h'))
for f in files:
    rel = f.relative_to(src)
    dirname = rel.parent
    stem = f.stem
    newstem = pascal_case(stem)
    if f.suffix == '.cpp':
        newext = '.cc'
    else:
        newext = '.h'
    newpath = src / dirname / (newstem + newext)
    mapping[str(f.relative_to(root))] = str(newpath.relative_to(root))

# Copy files to new names
for old, new in mapping.items():
    oldp = root / old
    newp = root / new
    newp.parent.mkdir(parents=True, exist_ok=True)
    print(f'Copying {old} -> {new}')
    shutil.copy2(oldp, newp)

# Replace includes across src
all_files = list(src.rglob('*.*'))
for f in all_files:
    if f.is_file():
        text = f.read_text(encoding='utf-8')
        changed = False
        for old, new in mapping.items():
            old_name = Path(old).name
            new_name = Path(new).name
            if old_name in text:
                text = text.replace(old_name, new_name)
                changed = True
        if changed:
            f.write_text(text, encoding='utf-8')
            print(f'Updated includes in {f.relative_to(root)}')

# Update references in repo files (CMakeLists, README etc)
other_files = [root / 'CMakeLists.txt', root / 'README.md']
for f in other_files:
    if f.exists():
        text = f.read_text(encoding='utf-8')
        changed = False
        for old, new in mapping.items():
            old_name = Path(old).name
            new_name = Path(new).name
            if old_name in text:
                text = text.replace(old_name, new_name)
                changed = True
        if changed:
            f.write_text(text, encoding='utf-8')
            print(f'Updated references in {f.name}')

# Remove old files
for old in mapping.keys():
    oldp = root / old
    if oldp.exists():
        print(f'Removing old file {old}')
        oldp.unlink()

print('Done')
