from PIL import Image
import numpy as np

def image_to_icon_array(image_path, element_size=1):
    """
    Convert an image to an icon array representation.
    
    :param image_path: Path to the image file.
    :param element_size: Size of each pixel block for the icon array (resolution adjustment).
    :return: A list representing the icon array.
    """
    img = Image.open(image_path).convert("L")
    img = img.resize((img.width // element_size, img.height // element_size))
    
    img_array = np.array(img)
    
    threshold = 128 
    binary_array = (img_array > threshold).astype(int)
    
    icon_array = []
    for row in binary_array:
        binary_row = ''.join(map(str, row))  
        icon_array.append(int(binary_row, 2))
    
    return icon_array, binary_array

def generate_c_file(icon_array, c_file_path, array_width):
    """
    Generate a C file with the icon array.
    
    :param icon_array: The icon array to include in the C file.
    :param c_file_path: Path to save the C file.
    :param array_width: Width of each row in the icon array.
    """
    with open(c_file_path, "w") as c_file:
        c_file.write("#include <stdio.h>\n\n")
        c_file.write(f"int icon[] = {{\n")
        
        for row in icon_array:
            c_file.write(f"    0b{row:0{array_width}b},\n")
        
        c_file.write("};\n\n")
        
        c_file.write("int main() {\n")
        c_file.write("    int icon_height = sizeof(icon) / sizeof(icon[0]);\n")
        c_file.write("    int icon_width = sizeof(icon[0]) * 8;\n")
        c_file.write("    for (int i = 0; i < icon_height; i++) {\n")
        c_file.write("        printf(\"Row %d: 0x%X\\n\", i, icon[i]);\n")
        c_file.write("    }\n")
        c_file.write("    return 0;\n")
        c_file.write("}\n")

if __name__ == "__main__":
    image_path = "boot/images/biboo_os.png" 
    element_size = 12  
    
    icon_array, binary_array = image_to_icon_array(image_path, element_size)
    
    c_file_path = "boot/images/icon_array.c"
    array_width = len(binary_array[0])
    
    generate_c_file(icon_array, c_file_path, array_width)
    print(f"C file generated: {c_file_path}")
