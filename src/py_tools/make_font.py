import freetype
import sys
import os
import argparse

parser = argparse.ArgumentParser(description='kOSeki convert TTF to C fontarray')
parser.add_argument('--font', default='Fuzzy.ttf', help='name of the ttf in src/fonts/')
parser.add_argument('--output', default=None, help='output path (default is boot/font.c)')

args = parser.parse_args()

font_filename = f'src/fonts/{args.font}' if not os.path.isabs(args.font) else args.font
font_name = font_filename.split("/")[-1].split(".")[0].lower()

font_width = 10
font_height = 15

print(f"Converting {font_filename} to C array with name '{font_name}'")
print(f"Font dimensions: {font_width}x{font_height}")

face = freetype.Face(font_filename)
face.set_pixel_sizes(font_width, font_height)

characters = []

for i in range(ord(' '), 127):
    face.load_char(chr(i), freetype.FT_LOAD_RENDER | freetype.FT_LOAD_TARGET_MONO)
    data = bytearray(face.glyph.bitmap.rows * face.glyph.bitmap.width)

    for y in range(face.glyph.bitmap.rows):
        for b in range(face.glyph.bitmap.pitch):
            byte_val = face.glyph.bitmap.buffer[y * face.glyph.bitmap.pitch + b]
            bits_traversed = b * 8
            row = y * face.glyph.bitmap.width + bits_traversed

            for b_index in range(min(8, face.glyph.bitmap.width - bits_traversed)):
                bit = byte_val & (1 << (7 - b_index))
                data[row + b_index] = 1 if bit else 0

    representation = ''
    for y in range(face.glyph.bitmap.rows):
        if y < font_height:
            representation += '\t\t0b'
            for x in range(face.glyph.bitmap.width):
                if x < font_width:
                    representation += '1' if data[y * face.glyph.bitmap.width + x] else '0'
            if face.glyph.bitmap.width < font_width:
                representation += ''.join(['0' for _ in range(font_width - face.glyph.bitmap.width)])
            representation += ',\n'

    extra = ''
    
    if face.glyph.bitmap.rows < font_height:
        for _ in range(font_height - face.glyph.bitmap.rows):
            extra += '\t\t0b' + ''.join(['0' for _ in range(font_width)]) + ',\n'

    representation = extra + representation
    characters.append(representation)

output_path = args.output if args.output else f'src/fonts/{font_name}.c'
utility_output_path = f'src/fonts/backup_{font_name}.c'

with open(utility_output_path, 'w') as file:
    count = 0
    array = 0
    file.write(f'unsigned int {font_name.lower()}_get_char(int index, int y) {{\n')
    for i, char_repr in enumerate(characters):
        if count == 0:
            file.write(f'unsigned int characters_{font_name.lower()}_{array}[][150] = {{\n')
        file.write('\t{\n')
        file.write(char_repr)
        file.write('\t},\n')

        count += 1
        if count >= 13:
            array += 1
            count = 0
            file.write('};\n//################################################################################\n')

    file.write('};\n')
    file.write('\tint start = (int)(\' \');\n')
    file.write(f'\tif (index >= start + 13 * 0 && index < start + 13 * 1) {{\n')
    file.write(f'\t\treturn characters_{font_name.lower()}_0[index - (start + 13 * 0)][y];\n')
    file.write('\t}\n')
    for idx in range(7):
        file.write(f'\telse if (index >= start + 13 * {idx + 1} && index < start + 13 * {idx + 2}) {{\n')
        file.write(f'\t\treturn characters_{font_name.lower()}_{idx + 1}[index - (start + 13 * {idx + 1})][y];\n')
        file.write('\t}\n')
    file.write('}\n')
    file.write(f'const int font_{font_name.lower()}_width = {font_width};\n')
    file.write(f'const int font_{font_name.lower()}_height = {font_height};\n')

print(f"saved to {utility_output_path}")

with open(utility_output_path, 'r') as source_file:
    font_code = source_file.read()

with open(output_path, 'w') as output_file:
    output_file.write('// Font array is generated from ' + os.path.basename(font_filename) + '\n')
    output_file.write('// SIZE: ' + str(font_width) + 'x' + str(font_height) + '\n\n')
    output_file.write(font_code)

print(f"source copied to {output_path}")
print("DONE")