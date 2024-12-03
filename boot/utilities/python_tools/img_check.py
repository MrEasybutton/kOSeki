import numpy as np
from PIL import Image

# Read the C array data from 'boot/image_data.c'
def read_c_array(filename):
    with open(filename, "r") as f:
        data = f.read()

    # Find the array initialization part and extract the values
    start = data.find("unsigned int image_data[] = {") + len("unsigned int image_data[] = {")
    end = data.find("};", start)
    array_str = data[start:end].strip()

    # Remove any non-numeric characters (commas, spaces) and convert to a list of integers
    image_data = [int(x) for x in array_str.replace(",", "").split() if x.isdigit()]

    return image_data

# Read the C array data
image_data = read_c_array("boot/image_data.c")

# Set the image dimensions (width and height)
width = 16  # Modify if necessary based on your image dimensions
height = 16  # Modify if necessary based on your image dimensions

# Prepare an empty list to store the pixel values
pixels = []

# Iterate over the image data in chunks of three to extract RGB values
for i in range(0, len(image_data), 3):
    # Extract the RGB components for the pixel
    r = image_data[i]
    g = image_data[i + 1]
    b = image_data[i + 2]
    
    # Append the pixel (RGB) to the list of pixels
    pixels.append((r, g, b))

# Convert the list of pixels into a numpy array with shape (height, width, 3)
pixels = np.array(pixels, dtype=np.uint8).reshape((height, width, 3))

# Create an image from the pixel data
image = Image.fromarray(pixels)

# Save the image to a file (you can adjust the filename and format)
image.save("output_biboo.png")

print("Image has been saved as 'output_biboo.png'")

# This is AI-generated because I did not have the patience to code the check. Basically it just converts your C array back into your image.
# If you're wondering why this isn't in kOSeki, it's because it is obsolete.