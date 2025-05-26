#!/usr/bin/env python3

import os
import re
import sys

def extract_cpp_example(readme_path, output_path):
    with open(readme_path, 'r') as f:
        content = f.read()
    
    # Find C++ code blocks (```cpp ... ```)
    cpp_blocks = re.findall(r'```cpp\n(.*?)\n```', content, re.DOTALL)
    
    if not cpp_blocks:
        print(f"No C++ code blocks found in {readme_path}")
        return False
    
    # We'll use the first complete example with main()
    main_examples = [block for block in cpp_blocks if "int main()" in block]
    
    if not main_examples:
        print(f"No complete examples with main() found in {readme_path}")
        return False
    
    # Add include for iostream if not present (for our status output)
    example = main_examples[0]
    
    # Create directory if it doesn't exist
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    
    with open(output_path, 'w') as f:
        f.write(example)

    return True

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} README.md output.cpp")
        sys.exit(1)
    
    if not extract_cpp_example(sys.argv[1], sys.argv[2]):
        sys.exit(1)
