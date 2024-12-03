from PIL import Image
import numpy as np
import os

image_path = "boot/biboo32.png"
image_name = os.path.splitext(os.path.basename(image_path))[0]
header_filename = f"boot/image_data_{image_name}.h"

def generate_header(image_path, header_filename):
    # Open the image using PIL (Pillow)
    image = Image.open(image_path)

    # Convert the image to RGB if it is not already in that mode
    image = image.convert("RGB")

    # Get the image size
    width, height = image.size

    # Define the number of sections you want to divide the image into (e.g., 16 sections)
    sections = 16
    num_cols = 4  # Number of columns per section (e.g., 4x4 grid)
    num_rows = 4  # Number of rows per section (e.g., 4x4 grid)

    # Calculate the width and height of each section
    section_width = width // num_cols
    section_height = height // num_rows

    # Open the header file for writing
    with open(header_filename, 'w') as f:
        # Write the header guard
        f.write(f"#ifndef IMAGE_DATA_{header_filename.split('.')[0].upper()}_H\n")
        f.write(f"#define IMAGE_DATA_{header_filename.split('.')[0].upper()}_H\n\n")

        # Generate extern declarations for each section
        for i in range(1, sections + 1):
            f.write(f"extern unsigned char image_data_{image_name}_{i}[];\n")

        # Declare the DrawImage function
        f.write("\n// Declare the function that draws the images\n")
        f.write(f"void DrawImage_{image_name}(int x, int y, int size);\n\n")

        # End the header guard
        f.write(f"#endif // IMAGE_DATA_{header_filename.split('.')[0].upper()}_H\n")

    print(f"Header file {header_filename} generated successfully!")


def extract_image_data(image_path, sections=16, num_cols=4, num_rows=4):
    # Open the image using PIL (Pillow)
    image = Image.open(image_path)
    image = image.convert("RGB")

    # Get the image size
    width, height = image.size

    # Calculate the width and height of each section
    section_width = width // num_cols
    section_height = height // num_rows

    image_data = []

    # Extract pixel data for each section
    for i in range(sections):
        row = i // num_cols
        col = i % num_cols

        # Define the bounding box for the current section
        left = col * section_width
        upper = row * section_height
        right = (col + 1) * section_width
        lower = (row + 1) * section_height

        # Crop the image to get the current section
        section = image.crop((left, upper, right, lower))

        # Convert the section to numpy array and flatten to a 1D list
        section_data = np.array(section).flatten().tolist()

        # Add the section data to the image_data list
        image_data.append(section_data)

    return image_data


def main():

    # Generate the header file with extern declarations
    generate_header(image_path, header_filename)

    # Extract the image data (for testing purposes)
    image_data = extract_image_data(image_path)

    # Optionally, print or save the image data arrays for debugging
    for i, data in enumerate(image_data, 1):
        print(f"image_data_{image_name}_{i}[] = {data};\n")


if __name__ == "__main__":
    main()

# This is AI-generated because I did not have the patience to code the conversion for this function. Basically it just generates a header based on how many C arrays we have to use for an image.
# If you're wondering why this isn't in kOSeki, it's because it's not worth the memory usage and probably a sign to implement proper memory management.