#ifndef PEBBLESHELL_H
#define PEBBLESHELL_H
#include "gui.h"

void pbsh_handle_scroll(Window* win, int scroll_delta);
void pbsh_m_down(Window* win, int x, int y);
void pbsh_m_up(Window* win, int x, int y);
void pbsh_m_move(Window* win, int x, int y);
void pbsh_cleanup(Window* win);
void launch_pbsh();

#endif