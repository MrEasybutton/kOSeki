#include <stdint.h>
#include <stddef.h>
#include "../graphics.h"

void DrawImage_simulacra(int x, int y, int size) {

    int pixel_where_x = x;

    int pixel_where_y = y;

    int pixel_index = 0;

    int pixel_size = size;

    unsigned char image_data_simulacra_1[] = {
246, 207, 213, 45, 45, 51, 45, 45, 51, 45, 45, 51};

    for (int col = 0; col < 4; col++) {
        // Extract the RGB values for the current pixel from image_data_simulacra_1
        int red = image_data_simulacra_1[pixel_index * 3];         // Red value
        int green = image_data_simulacra_1[pixel_index * 3 + 1];   // Green value
        int blue = image_data_simulacra_1[pixel_index * 3 + 2];    // Blue value

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

    unsigned char image_data_simulacra_2[] = {
45, 45, 51, 33, 33, 39, 33, 33, 39, 33, 33, 39};

    for (int col = 0; col < 4; col++) {
        // Extract the RGB values for the current pixel from image_data_simulacra_2
        int red = image_data_simulacra_2[pixel_index * 3];         // Red value
        int green = image_data_simulacra_2[pixel_index * 3 + 1];   // Green value
        int blue = image_data_simulacra_2[pixel_index * 3 + 2];    // Blue value

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

    unsigned char image_data_simulacra_3[] = {
45, 45, 51, 33, 33, 39, 255, 218, 220, 185, 24, 33};

    for (int col = 0; col < 4; col++) {
        // Extract the RGB values for the current pixel from image_data_simulacra_3
        int red = image_data_simulacra_3[pixel_index * 3];         // Red value
        int green = image_data_simulacra_3[pixel_index * 3 + 1];   // Green value
        int blue = image_data_simulacra_3[pixel_index * 3 + 2];    // Blue value

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

    unsigned char image_data_simulacra_4[] = {
45, 45, 51, 33, 33, 39, 185, 24, 33, 185, 24, 33};

    for (int col = 0; col < 4; col++) {
        // Extract the RGB values for the current pixel from image_data_simulacra_4
        int red = image_data_simulacra_4[pixel_index * 3];         // Red value
        int green = image_data_simulacra_4[pixel_index * 3 + 1];   // Green value
        int blue = image_data_simulacra_4[pixel_index * 3 + 2];    // Blue value

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
