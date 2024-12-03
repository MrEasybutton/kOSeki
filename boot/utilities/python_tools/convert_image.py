from PIL import Image
import numpy as np
import os

image_path = 'boot/biboo32.png'  # Replace with the path to your 16x16 image
fcimg = 8
image_name = os.path.splitext(os.path.basename(image_path))[0]
output_c_file = f'boot/images/image_data_{image_name}.c'  # This will be the C file generated

def image_to_c_function(image_path, output_c_file):
    # Load the image (ensure it's in RGB mode)
    img = Image.open(image_path).convert('RGB')

    # Resize the image to 16x16 just in case it's not the correct size
    img = img.resize((fcimg, fcimg))

    # Convert the image to a NumPy array of shape (16, 16, 3) for RGB
    img_array = np.array(img)

    # Open the C file for writing
    with open(output_c_file, 'w') as c_file:
        c_file.write("""#include <stdint.h>
#include <stddef.h>
#include "../graphics.h"\n\n""")
        
        # Write the function signature with the image name
        c_file.write(f"void DrawImage_{image_name}(int x, int y, int size) {{\n\n")

        c_file.write("""    int pixel_where_x = x;\n
    int pixel_where_y = y;\n
    int pixel_index = 0;\n
    int pixel_size = size;\n\n""")
        
        # Process the image in chunks of 48 pixels (16x3 for RGB values)
        pixel_index = 0
        pixel_values = []
        for row in range(fcimg):
            for col in range(fcimg):
                r, g, b = img_array[row, col]  # Extract RGB
                pixel_values.extend([r, g, b])  # Add RGB values to the list

        # For each image_data1 to image_data16, generate hardcoded drawing code
        for i in range(fcimg):
            start_index = i * fcimg * 3
            end_index = start_index + fcimg * 3
            current_pixel_values = pixel_values[start_index:end_index]

            # Write the image data array with the unique name for each image
            c_file.write(f"    unsigned char image_data_{image_name}_{i+1}[] = {{\n")
            c_file.write(", ".join(map(str, current_pixel_values)) + "};\n\n")

            # Hardcode the drawing loop for this image_data with the unique suffix
            c_file.write(f"    for (int col = 0; col < {fcimg}; col++) {{\n")
            c_file.write(f"        // Extract the RGB values for the current pixel from image_data_{image_name}_{i+1}\n")
            c_file.write(f"        int red = image_data_{image_name}_{i+1}[pixel_index * 3];         // Red value\n")
            c_file.write(f"        int green = image_data_{image_name}_{i+1}[pixel_index * 3 + 1];   // Green value\n")
            c_file.write(f"        int blue = image_data_{image_name}_{i+1}[pixel_index * 3 + 2];    // Blue value\n")
            c_file.write(f"\n")
            c_file.write(f"        // Draw the pixel at the appropriate position\n")
            c_file.write(f"        DrawRect(pixel_where_x, pixel_where_y, pixel_size, pixel_size, red, green, blue);\n")
            c_file.write(f"\n")
            c_file.write(f"        // Move to the next pixel on the x-axis\n")
            c_file.write(f"        pixel_where_x += pixel_size;  // Move right by pixel_size pixels for the next pixel\n")
            c_file.write(f"\n")
            c_file.write(f"        // Increment the pixel index\n")
            c_file.write(f"        pixel_index++;\n")
            c_file.write(f"    }}\n\n")

            # Reset pixel position for the next row
            c_file.write(f"    pixel_where_x = x;      // Reset X to starting position for the new row\n")
            c_file.write(f"    pixel_where_y += pixel_size;     // Move Y down by pixel_size pixels to start the next row\n")
            c_file.write(f"    pixel_index = 0;  // Reset pixel index for the next image\n\n")
        
        # Close the function
        c_file.write("}\n")


image_to_c_function(image_path, output_c_file)

# This is AI-generated because I did not have the patience to code the conversion for this function. Basically it just converts your image into several C arrays, with customisable resolution.
# If you're wondering why this isn't in kOSeki, it's because it's not worth the memory usage and probably a sign to implement proper memory management.