#include <stdint.h>
#include <stddef.h>
#include "../graphics.h"

void DrawImage_foundyou(int x, int y, int size) {

    int pixel_where_x = x;

    int pixel_where_y = y;

    int pixel_index = 0;

    int pixel_size = size;

    unsigned char image_data_foundyou_1[] = {
33, 46, 65, 13, 12, 31, 9, 9, 30, 29, 38, 56};

    for (int col = 0; col < 4; col++) {
        // Extract the RGB values for the current pixel from image_data_foundyou_1
        int red = image_data_foundyou_1[pixel_index * 3];         // Red value
        int green = image_data_foundyou_1[pixel_index * 3 + 1];   // Green value
        int blue = image_data_foundyou_1[pixel_index * 3 + 2];    // Blue value

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

    unsigned char image_data_foundyou_2[] = {
34, 38, 54, 67, 64, 92, 60, 57, 86, 17, 22, 39};

    for (int col = 0; col < 4; col++) {
        // Extract the RGB values for the current pixel from image_data_foundyou_2
        int red = image_data_foundyou_2[pixel_index * 3];         // Red value
        int green = image_data_foundyou_2[pixel_index * 3 + 1];   // Green value
        int blue = image_data_foundyou_2[pixel_index * 3 + 2];    // Blue value

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

    unsigned char image_data_foundyou_3[] = {
34, 37, 51, 106, 100, 133, 107, 103, 138, 38, 36, 54};

    for (int col = 0; col < 4; col++) {
        // Extract the RGB values for the current pixel from image_data_foundyou_3
        int red = image_data_foundyou_3[pixel_index * 3];         // Red value
        int green = image_data_foundyou_3[pixel_index * 3 + 1];   // Green value
        int blue = image_data_foundyou_3[pixel_index * 3 + 2];    // Blue value

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

    unsigned char image_data_foundyou_4[] = {
40, 45, 58, 55, 68, 98, 69, 86, 124, 49, 56, 81};

    for (int col = 0; col < 4; col++) {
        // Extract the RGB values for the current pixel from image_data_foundyou_4
        int red = image_data_foundyou_4[pixel_index * 3];         // Red value
        int green = image_data_foundyou_4[pixel_index * 3 + 1];   // Green value
        int blue = image_data_foundyou_4[pixel_index * 3 + 2];    // Blue value

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
