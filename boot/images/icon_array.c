#include <stdio.h>

int icon[] = {
    0b111111111111111111111,
    0b111111111111111111111,
    0b111111111111111111111,
    0b111111111111111111111,
    0b111111111111111111111,
    0b111111111111111111111,
    0b111111111111111111111,
    0b111111111111111111111,
    0b111111111111111111111,
    0b111111111111111111111,
    0b111111111111111111111,
    0b111111111111111111111,
    0b111111111111111111111,
    0b111111111111111111111,
    0b111111111111111111111,
    0b111111111111111111111,
    0b111111111111111111111,
    0b111111111111111111111,
    0b111111111111111111111,
    0b111111111111111111111,
    0b111111111111111111111,
};

int main() {
    int icon_height = sizeof(icon) / sizeof(icon[0]);
    int icon_width = sizeof(icon[0]) * 8;
    for (int i = 0; i < icon_height; i++) {
        printf("Row %d: 0x%X\n", i, icon[i]);
    }
    return 0;
}