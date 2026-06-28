import math

def generate_sincos_tables(degrees=360):
    sin_table = []
    cos_table = []
    for i in range(degrees):
        rad = math.radians(i)
        sin_val = math.sin(rad)
        cos_val = math.cos(rad)
        if sin_val == -0.0:
            sin_val = 0.0
        if cos_val == -0.0:
            cos_val = 0.0
        sin_table.append(sin_val)
        cos_table.append(cos_val)
    
    c_code = "#include <include/math.h>\n\n"
    c_code += "static const float sin_lookup[] = {\n"
    for i, val in enumerate(sin_table):
        c_code += f"    {val}f,"
        if (i + 1) % 10 == 0:
            c_code += "\n"
    c_code += "\n};\n\n"
    
    c_code += "static const float cos_lookup[] = {\n"
    for i, val in enumerate(cos_table):
        c_code += f"    {val}f,"
        if (i + 1) % 10 == 0:
            c_code += "\n"
    c_code += "\n};\n\n"

    c_code += """
float sin_deg(int angle) {
    angle %= 360;
    if (angle < 0) {
        angle += 360;
    }
    return sin_lookup[angle];
}

float cos_deg(int angle) {
    angle %= 360;
    if (angle < 0) {
        angle += 360;
    }
    return cos_lookup[angle];
}
"""
    return c_code

print(generate_sincos_tables())
