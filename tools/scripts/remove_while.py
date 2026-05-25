#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import os

def replace_while_in_tk(code):
    result = []
    i = 0
    n = len(code)
    while i < n:
        # Check for block comments
        if i + 1 < n and code[i:i+2] == '/*':
            end = code.find('*/', i + 2)
            if end == -1:
                result.append(code[i:])
                break
            else:
                result.append(code[i:end+2])
                i = end + 2
                continue
        # Check for line comments
        elif i + 1 < n and code[i:i+2] == '//':
            eol = code.find('\n', i)
            if eol == -1:
                result.append(code[i:])
                break
            else:
                result.append(code[i:eol])
                i = eol
                continue
        # Check for string literals
        elif code[i] == '"':
            start = i
            i += 1
            while i < n:
                if code[i] == '"' and code[i-1] != '\\':
                    i += 1
                    break
                i += 1
            result.append(code[start:i])
            continue
        elif code[i] == "'":
            # Check if it is a character literal
            is_char_lit = False
            if i + 2 < n and code[i+2] == "'":
                is_char_lit = True
            elif i + 3 < n and code[i+1] == '\\' and code[i+3] == "'":
                is_char_lit = True
                
            if is_char_lit:
                start = i
                i += 1
                while i < n:
                    if code[i] == "'" and code[i-1] != '\\':
                        i += 1
                        break
                    i += 1
                result.append(code[start:i])
                continue
            else:
                result.append(code[i])
                i += 1
                continue
        # Check for 'while' keyword
        elif code[i:i+5] == 'while':
            is_word_start = (i == 0 or not (code[i-1].isalnum() or code[i-1] == '_'))
            is_word_end = (i + 5 >= n or not (code[i+5].isalnum() or code[i+5] == '_'))
            if is_word_start and is_word_end:
                result.append('loop')
                i += 5
                continue
        
        result.append(code[i])
        i += 1
        
    return "".join(result)

def migrate_tests():
    test_dirs = ['tests/pass', 'tests/fail', 'lib']
    modified_count = 0
    for test_dir in test_dirs:
        if not os.path.exists(test_dir):
            continue
        for root, dirs, files in os.walk(test_dir):
            for file in files:
                if file == 'while_abolished.tk':
                    continue
                if file.endswith('.tk'):
                    filepath = os.path.join(root, file)
                    with open(filepath, 'r', encoding='utf-8') as f:
                        content = f.read()
                    
                    new_content = replace_while_in_tk(content)
                    if new_content != content:
                        with open(filepath, 'w', encoding='utf-8') as f:
                            f.write(new_content)
                        print(f"Migrated: {filepath}")
                        modified_count += 1
    print(f"Successfully migrated {modified_count} test files.")

if __name__ == '__main__':
    migrate_tests()
