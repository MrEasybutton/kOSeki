#include <stdint.h>
#include <stddef.h>
#include "../graphics.h"

void DrawImage_biboo32(int x, int y, int size) {

    int pixel_where_x = x;

    int pixel_where_y = y;

    int pixel_index = 0;

    int pixel_size = size;

    unsigned char image_data_biboo32_1[] = {
194, 176, 211, 206, 188, 224, 98, 86, 109, 44, 26, 66, 173, 127, 229, 173, 127, 229, 44, 26, 66, 98, 86, 109, 206, 188, 224, 194, 176, 211};

    for (int col = 0; col < 10; col++) {
        // Extract the RGB values for the current pixel from image_data_biboo32_1
        int red = image_data_biboo32_1[pixel_index * 3];         // Red value
        int green = image_data_biboo32_1[pixel_index * 3 + 1];   // Green value
        int blue = image_data_biboo32_1[pixel_index * 3 + 2];    // Blue value

        // Draw the pixel at the appropriate position
        DrawRect(pixel_where_x, pixel_where_y, pixel_size, pixel_size, red, green, blue);

        // Move to the next pixel on the x-axis
        pixel_where_x += pixel_size;  // Move right by pixel_size pixels for the next pixel

        // Increment the pixel index
        pixel_index++;
    }

    pixel_where_x = x;      // Reset X to starting position for the new row
    pixel_where_y += pixel_size;     // Move Y down by pixel_size pixels to start the next row
    pixel_index = 0;  // Reset pixel index for the next image

    unsigned char image_data_biboo32_2[] = {
194, 176, 211, 207, 189, 225, 88, 80, 104, 48, 11, 45, 226, 103, 189, 226, 103, 189, 48, 11, 45, 88, 80, 104, 207, 189, 225, 194, 176, 211};

    for (int col = 0; col < 10; col++) {
        // Extract the RGB values for the current pixel from image_data_biboo32_2
        int red = image_data_biboo32_2[pixel_index * 3];         // Red value
        int green = image_data_biboo32_2[pixel_index * 3 + 1];   // Green value
        int blue = image_data_biboo32_2[pixel_index * 3 + 2];    // Blue value

        // Draw the pixel at the appropriate position
        DrawRect(pixel_where_x, pixel_where_y, pixel_size, pixel_size, red, green, blue);

        // Move to the next pixel on the x-axis
        pixel_where_x += pixel_size;  // Move right by pixel_size pixels for the next pixel

        // Increment the pixel index
        pixel_index++;
    }

    pixel_where_x = x;      // Reset X to starting position for the new row
    pixel_where_y += pixel_size;     // Move Y down by pixel_size pixels to start the next row
    pixel_index = 0;  // Reset pixel index for the next image

    unsigned char image_data_biboo32_3[] = {
194, 176, 211, 199, 180, 216, 148, 135, 167, 100, 70, 102, 110, 31, 76, 110, 31, 76, 100, 70, 102, 148, 135, 167, 199, 180, 216, 194, 176, 211};

    for (int col = 0; col < 10; col++) {
        // Extract the RGB values for the current pixel from image_data_biboo32_3
        int red = image_data_biboo32_3[pixel_index * 3];         // Red value
        int green = image_data_biboo32_3[pixel_index * 3 + 1];   // Green value
        int blue = image_data_biboo32_3[pixel_index * 3 + 2];    // Blue value

        // Draw the pixel at the appropriate position
        DrawRect(pixel_where_x, pixel_where_y, pixel_size, pixel_size, red, green, blue);

        // Move to the next pixel on the x-axis
        pixel_where_x += pixel_size;  // Move right by pixel_size pixels for the next pixel

        // Increment the pixel index
        pixel_index++;
    }

    pixel_where_x = x;      // Reset X to starting position for the new row
    pixel_where_y += pixel_size;     // Move Y down by pixel_size pixels to start the next row
    pixel_index = 0;  // Reset pixel index for the next image

    unsigned char image_data_biboo32_4[] = {
193, 175, 210, 204, 187, 220, 223, 205, 238, 172, 159, 188, 43, 41, 60, 43, 41, 60, 172, 159, 188, 223, 205, 238, 204, 187, 220, 193, 175, 210};

    for (int col = 0; col < 10; col++) {
        // Extract the RGB values for the current pixel from image_data_biboo32_4
        int red = image_data_biboo32_4[pixel_index * 3];         // Red value
        int green = image_data_biboo32_4[pixel_index * 3 + 1];   // Green value
        int blue = image_data_biboo32_4[pixel_index * 3 + 2];    // Blue value

        // Draw the pixel at the appropriate position
        DrawRect(pixel_where_x, pixel_where_y, pixel_size, pixel_size, red, green, blue);

        // Move to the next pixel on the x-axis
        pixel_where_x += pixel_size;  // Move right by pixel_size pixels for the next pixel

        // Increment the pixel index
        pixel_index++;
    }

    pixel_where_x = x;      // Reset X to starting position for the new row
    pixel_where_y += pixel_size;     // Move Y down by pixel_size pixels to start the next row
    pixel_index = 0;  // Reset pixel index for the next image

    unsigned char image_data_biboo32_5[] = {
195, 176, 211, 205, 192, 216, 220, 210, 229, 220, 207, 232, 200, 181, 218, 200, 181, 218, 220, 207, 232, 220, 210, 229, 205, 192, 216, 195, 176, 211};

    for (int col = 0; col < 10; col++) {
        // Extract the RGB values for the current pixel from image_data_biboo32_5
        int red = image_data_biboo32_5[pixel_index * 3];         // Red value
        int green = image_data_biboo32_5[pixel_index * 3 + 1];   // Green value
        int blue = image_data_biboo32_5[pixel_index * 3 + 2];    // Blue value

        // Draw the pixel at the appropriate position
        DrawRect(pixel_where_x, pixel_where_y, pixel_size, pixel_size, red, green, blue);

        // Move to the next pixel on the x-axis
        pixel_where_x += pixel_size;  // Move right by pixel_size pixels for the next pixel

        // Increment the pixel index
        pixel_index++;
    }

    pixel_where_x = x;      // Reset X to starting position for the new row
    pixel_where_y += pixel_size;     // Move Y down by pixel_size pixels to start the next row
    pixel_index = 0;  // Reset pixel index for the next image

    unsigned char image_data_biboo32_6[] = {
228, 202, 218, 50, 44, 47, 104, 100, 109, 234, 219, 246, 194, 175, 211, 194, 175, 211, 234, 219, 246, 104, 100, 109, 50, 44, 47, 228, 202, 218};

    for (int col = 0; col < 10; col++) {
        // Extract the RGB values for the current pixel from image_data_biboo32_6
        int red = image_data_biboo32_6[pixel_index * 3];         // Red value
        int green = image_data_biboo32_6[pixel_index * 3 + 1];   // Green value
        int blue = image_data_biboo32_6[pixel_index * 3 + 2];    // Blue value

        // Draw the pixel at the appropriate position
        DrawRect(pixel_where_x, pixel_where_y, pixel_size, pixel_size, red, green, blue);

        // Move to the next pixel on the x-axis
        pixel_where_x += pixel_size;  // Move right by pixel_size pixels for the next pixel

        // Increment the pixel index
        pixel_index++;
    }

    pixel_where_x = x;      // Reset X to starting position for the new row
    pixel_where_y += pixel_size;     // Move Y down by pixel_size pixels to start the next row
    pixel_index = 0;  // Reset pixel index for the next image

    unsigned char image_data_biboo32_7[] = {
226, 200, 211, 173, 153, 160, 176, 160, 179, 203, 186, 220, 194, 175, 211, 194, 175, 211, 203, 186, 220, 176, 160, 179, 173, 153, 160, 226, 200, 211};

    for (int col = 0; col < 10; col++) {
        // Extract the RGB values for the current pixel from image_data_biboo32_7
        int red = image_data_biboo32_7[pixel_index * 3];         // Red value
        int green = image_data_biboo32_7[pixel_index * 3 + 1];   // Green value
        int blue = image_data_biboo32_7[pixel_index * 3 + 2];    // Blue value

        // Draw the pixel at the appropriate position
        DrawRect(pixel_where_x, pixel_where_y, pixel_size, pixel_size, red, green, blue);

        // Move to the next pixel on the x-axis
        pixel_where_x += pixel_size;  // Move right by pixel_size pixels for the next pixel

        // Increment the pixel index
        pixel_index++;
    }

    pixel_where_x = x;      // Reset X to starting position for the new row
    pixel_where_y += pixel_size;     // Move Y down by pixel_size pixels to start the next row
    pixel_index = 0;  // Reset pixel index for the next image

    unsigned char image_data_biboo32_8[] = {
206, 186, 209, 219, 196, 220, 207, 187, 217, 190, 173, 209, 194, 176, 211, 194, 176, 211, 190, 173, 209, 207, 187, 217, 219, 196, 220, 206, 186, 209};

    for (int col = 0; col < 10; col++) {
        // Extract the RGB values for the current pixel from image_data_biboo32_8
        int red = image_data_biboo32_8[pixel_index * 3];         // Red value
        int green = image_data_biboo32_8[pixel_index * 3 + 1];   // Green value
        int blue = image_data_biboo32_8[pixel_index * 3 + 2];    // Blue value

        // Draw the pixel at the appropriate position
        DrawRect(pixel_where_x, pixel_where_y, pixel_size, pixel_size, red, green, blue);

        // Move to the next pixel on the x-axis
        pixel_where_x += pixel_size;  // Move right by pixel_size pixels for the next pixel

        // Increment the pixel index
        pixel_index++;
    }

    pixel_where_x = x;      // Reset X to starting position for the new row
    pixel_where_y += pixel_size;     // Move Y down by pixel_size pixels to start the next row
    pixel_index = 0;  // Reset pixel index for the next image

    unsigned char image_data_biboo32_9[] = {
192, 175, 211, 192, 175, 211, 193, 175, 211, 194, 176, 211, 194, 176, 211, 194, 176, 211, 194, 176, 211, 193, 175, 211, 192, 175, 211, 192, 175, 211};

    for (int col = 0; col < 10; col++) {
        // Extract the RGB values for the current pixel from image_data_biboo32_9
        int red = image_data_biboo32_9[pixel_index * 3];         // Red value
        int green = image_data_biboo32_9[pixel_index * 3 + 1];   // Green value
        int blue = image_data_biboo32_9[pixel_index * 3 + 2];    // Blue value

        // Draw the pixel at the appropriate position
        DrawRect(pixel_where_x, pixel_where_y, pixel_size, pixel_size, red, green, blue);

        // Move to the next pixel on the x-axis
        pixel_where_x += pixel_size;  // Move right by pixel_size pixels for the next pixel

        // Increment the pixel index
        pixel_index++;
    }

    pixel_where_x = x;      // Reset X to starting position for the new row
    pixel_where_y += pixel_size;     // Move Y down by pixel_size pixels to start the next row
    pixel_index = 0;  // Reset pixel index for the next image

    unsigned char image_data_biboo32_10[] = {
194, 176, 211, 194, 176, 211, 194, 176, 211, 194, 176, 211, 194, 176, 211, 194, 176, 211, 194, 176, 211, 194, 176, 211, 194, 176, 211, 194, 176, 211};

    for (int col = 0; col < 10; col++) {
        // Extract the RGB values for the current pixel from image_data_biboo32_10
        int red = image_data_biboo32_10[pixel_index * 3];         // Red value
        int green = image_data_biboo32_10[pixel_index * 3 + 1];   // Green value
        int blue = image_data_biboo32_10[pixel_index * 3 + 2];    // Blue value

        // Draw the pixel at the appropriate position
        DrawRect(pixel_where_x, pixel_where_y, pixel_size, pixel_size, red, green, blue);

        // Move to the next pixel on the x-axis
        pixel_where_x += pixel_size;  // Move right by pixel_size pixels for the next pixel

        // Increment the pixel index
        pixel_index++;
    }

    pixel_where_x = x;      // Reset X to starting position for the new row
    pixel_where_y += pixel_size;     // Move Y down by pixel_size pixels to start the next row
    pixel_index = 0;  // Reset pixel index for the next image

}
