import sys
import os

def file_to_cpp_header(filename, var_name, output_filename):
    if not os.path.exists(filename):
        print(f"Error: {filename} does not exist!")
        return False
        
    print(f"Embedding {filename} into {output_filename}...")
    with open(filename, 'rb') as f:
        data = f.read()
        
    os.makedirs(os.path.dirname(output_filename), exist_ok=True)
    
    with open(output_filename, 'w') as f:
        f.write(f"// Generated from {filename}\n")
        f.write("#pragma once\n\n")
        f.write(f"inline const unsigned char {var_name}[] = {{\n")
        
        hex_data = []
        for i, b in enumerate(data):
            hex_data.append(f"0x{b:02x}")
            if i % 12 == 11:
                f.write(", ".join(hex_data) + ",\n")
                hex_data = []
        if hex_data:
            f.write(", ".join(hex_data) + "\n")
        else:
            if f.tell() >= 2:
                f.seek(f.tell() - 2)
                f.write("\n")
            
        f.write("};\n\n")
        f.write(f"inline const unsigned int {var_name}_size = {len(data)};\n")
    return True

if __name__ == "__main__":
    if len(sys.argv) < 5:
        print("Usage: embed_assets.py <roboto_ttf> <fa_ttf> <controller_png> <output_dir>")
        sys.exit(1)
        
    roboto_ttf = sys.argv[1]
    fa_ttf = sys.argv[2]
    controller_png = sys.argv[3]
    output_dir = sys.argv[4]
    
    file_to_cpp_header(roboto_ttf, "roboto_medium_ttf", os.path.join(output_dir, "roboto_medium_ttf.h"))
    file_to_cpp_header(fa_ttf, "fa_solid_900_ttf", os.path.join(output_dir, "fa_solid_900_ttf.h"))
    file_to_cpp_header(controller_png, "dualsense_solid_black_png", os.path.join(output_dir, "dualsense_solid_black_png.h"))
