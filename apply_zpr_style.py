#!/usr/bin/env python3
"""
Apply ZPR Coding Style to C++ project.
Based on guidelines by Rafał Biedrzycki.

Usage: python3 apply_zpr_style.py [--file <path>] [--check-only]
"""

import os
import re
import sys
from pathlib import Path
from typing import List, Tuple

class ZprStyleApplier:
    """Apply ZPR coding style to C++ files."""
    
    def __init__(self):
        """Initialize the style applier."""
        self.changes_made = 0
        self.files_processed = 0
    
    def apply_control_statement_spacing(self, content: str) -> str:
        """Apply spacing to control statements: if( x ), for( ; ; ), etc."""
        # Match control statements and add spaces inside parentheses
        patterns = [
            (r'\bif\s*\(\s*', 'if( '),
            (r'\belse\s+if\s*\(\s*', 'else if( '),
            (r'\bwhile\s*\(\s*', 'while( '),
            (r'\bfor\s*\(\s*', 'for( '),
            (r'\bswitch\s*\(\s*', 'switch( '),
            (r'\)\s*\{', ' ){'),
        ]
        
        result = content
        for pattern, replacement in patterns:
            result = re.sub(pattern, replacement, result)
        
        # Fix closing parens for control statements
        result = re.sub(r'\)\s*;', ' );', result)
        
        return result
    
    def update_file_header(self, content: str, filename: str) -> str:
        """Ensure file has proper header block."""
        if not content.startswith('/**'):
            # Add header if missing
            header = f'''/**
 * @file {filename}
 * @brief [Description needed]
 * @author [Author needed]
 */
'''
            content = header + content
        return content
    
    def format_file(self, filepath: str, check_only: bool = False) -> bool:
        """Format a single C++ file according to ZPR style."""
        try:
            with open(filepath, 'r', encoding='utf-8') as f:
                original_content = f.read()
            
            content = original_content
            filename = os.path.basename(filepath)
            
            # Apply transformations
            content = self.apply_control_statement_spacing(content)
            content = self.update_file_header(content, filename)
            
            # Check if changes were made
            if content != original_content:
                if not check_only:
                    with open(filepath, 'w', encoding='utf-8') as f:
                        f.write(content)
                    print(f"✓ Formatted: {filepath}")
                else:
                    print(f"◆ Changes needed: {filepath}")
                self.changes_made += 1
                return True
            else:
                print(f"✓ Already compliant: {filepath}")
                return False
        
        except Exception as e:
            print(f"✗ Error processing {filepath}: {e}", file=sys.stderr)
            return False
        finally:
            self.files_processed += 1
    
    def process_directory(self, directory: str, check_only: bool = False) -> None:
        """Process all C++ files in directory."""
        cpp_files = list(Path(directory).rglob('*.cpp')) + \
                    list(Path(directory).rglob('*.hpp')) + \
                    list(Path(directory).rglob('*.h'))
        
        if not cpp_files:
            print(f"No C++ files found in {directory}")
            return
        
        print(f"Processing {len(cpp_files)} files...")
        for filepath in sorted(cpp_files):
            self.format_file(str(filepath), check_only)
        
        print(f"\nSummary: {self.files_processed} files processed, {self.changes_made} modified")


def main():
    """Main entry point."""
    check_only = '--check-only' in sys.argv
    
    # Find src directory
    zpr_root = Path(__file__).parent
    src_dir = zpr_root / 'src'
    
    if not src_dir.exists():
        print(f"Error: src directory not found at {src_dir}")
        sys.exit(1)
    
    applier = ZprStyleApplier()
    applier.process_directory(str(src_dir), check_only)
    
    if check_only:
        print("\nRun without --check-only to apply changes")


if __name__ == '__main__':
    main()
