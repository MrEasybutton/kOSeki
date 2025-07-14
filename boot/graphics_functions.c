int DrawButton(int x, int y, int w, int h, int r, int g, int b, char* text, int tr, int tg, int tb, int pid, int _, int __, int ___) {
    int hover = (curr_mouse_target == pid && mx > x && mx < x + w && my > y && my < y + h);
    int pressed = hover && left_clicked;

    int hr = r + 60 > 255 ? 235 : r + 60, hg = g + 60 > 255 ? 225 : g + 60, hb = b + 60 > 255 ? 245 : b + 60;
    int sr = r - 70 < 0 ? 0 : r - 70, sg = g - 70 < 0 ? 0 : g - 70, sb = b - 70 < 0 ? 0 : b - 70;
    int mr = r + 30 > 255 ? 255 : r + 30, mg = g + 30 > 255 ? 255 : g + 30, mb = b + 30 > 255 ? 255 : b + 30;

    int tsr = tr / 2, tsg = tg / 2, tsb = tb / 2;
    int tx = x + w / 10, ty = y + h / 10;

    DrawRect(x, y, w, h, r, g, b);

    if (pressed || hover) {
        int t = pressed ? 3 : 2;
        int h_color = pressed ? sr : hr;
        int s_color = pressed ? hr : sr;

        DrawRect(x, y, w, t, h_color, hg, hb);
        DrawRect(x, y, t, h, h_color, hg, hb);
        DrawRect(x, y + h - t, w, t, s_color, sg, sb);
        DrawRect(x + w - t, y, t, h, s_color, sg, sb);

        if (pressed)
            DrawRect(x + 3, y + 3, w - 6, h - 6, sr + 20, sg + 20, sb + 20);
        else
            DrawRectGradient(x + 3, y + 3, w - 6, h - 6, mr, mg, mb, r, g, b);

        if (!pressed) DrawText(getFontCharacter, font_font_width, font_font_height, text, tx + 1, ty + 1, tsr, tsg, tsb);
        DrawText(getFontCharacter, font_font_width, font_font_height, text, tx, ty, tr, tg, tb);
    } else {
        DrawRect(x, y, w, 2, hr, hg, hb);
        DrawRect(x, y, 2, h, hr, hg, hb);
        DrawRect(x, y + h - 2, w, 2, sr, sg, sb);
        DrawRect(x + w - 2, y, 2, h, sr, sg, sb);
        DrawRectGradient(x + 2, y + 2, w - 4, h - 4, r, g, b, sr + 40, sg + 40, sb + 40);
        DrawText(getFontCharacter, font_font_width, font_font_height, text, tx, ty, tr, tg, tb);
    }

    if (hover && left_clicked) {
        left_clicked = FALSE;
        return 1;
    }
    return 0;
}

int DrawX(int x, int y, int w, int h, int r, int g, int b, int pid) {
    return DrawButton(x, y, w, h, r, g, b, "", 255, 255, 255, pid, 0, 0, 0);
}

int DrawSlider(int x, int y, int w, int h, int min_val, int max_val, int* val, int r, int g, int b, int pid) {
    int hover = (curr_mouse_target == pid && mx >= x && mx <= x + w && my >= y && my <= y + h);
    int dragging = hover && left_clicked, changed = 0;

    *val = (*val < min_val) ? min_val : (*val > max_val ? max_val : *val);

    int hr = r + 60 > 255 ? 255 : r + 60, hg = g + 60 > 255 ? 255 : g + 60, hb = b + 60 > 255 ? 255 : b + 60;
    int sr = r - 70 < 0 ? 0 : r - 70, sg = g - 70 < 0 ? 0 : g - 70, sb = b - 70 < 0 ? 0 : b - 70;

    int range = max_val - min_val, hw = h, tw = w - hw;
    int hpos = x + ((*val - min_val) * tw) / range;

    if (dragging) {
        int nhpos = mx - hw / 2;
        nhpos = (nhpos < x) ? x : (nhpos > x + tw ? x + tw : nhpos);

        int nval = min_val + ((nhpos - x) * range) / tw;
        if (*val != nval) { *val = nval; changed = 1; }

        hpos = nhpos;
    }

    DrawRect(x, y + h / 2 - 2, w, 4, sr, sg, sb);
    int fill = hpos - x + hw / 2;
    if (fill > 0) DrawRect(x, y + h / 2 - 2, fill, 4, hr, hg, hb);

    int t = hover || dragging ? 2 : 2;
    int cr = hover || dragging ? hr : r, cg = hover || dragging ? hg : g, cb = hover || dragging ? hb : b;

    DrawRect(hpos, y, hw, h, cr, cg, cb);
    DrawRect(hpos, y, hw, t, r, g, b);
    DrawRect(hpos, y, t, h, r, g, b);
    DrawRect(hpos, y + h - t, hw, t, sr, sg, sb);
    DrawRect(hpos + hw - t, y, t, h, sr, sg, sb);

    return changed;
}

int DrawWindow(int* x, int* y, int* width, int* height, int r, int g, int b, int* mouse_held, int process_inst) {
    VBEInfoBlock* VBE = (VBEInfoBlock*) VBEInfoAddress;
    if (!left_clicked) *mouse_held = FALSE;

    int w = *width, h = *height;
    int x_res = VBE->x_resolution, y_res = VBE->y_resolution;

    if (curr_mouse_target == process_inst && (*mouse_held || 
        (left_clicked && mx > *x && mx < *x + w - 30 && my > *y && my < *y + 20))) {
        left_clicked = FALSE;
        *mouse_held = TRUE;
        *x = mx - w / 2;
        *y = my - 10;
    }

    if (*x < 65) *x = 65;
    else if (*x + w > x_res) *x = x_res - (w + 10);

    if (*y < 5) *y = 5;
    else if (*y + h > y_res) *y = y_res - (h + 10);

    int br = r > 39 ? r - 40 : 0, bg = g > 39 ? g - 40 : 0, bb = b > 39 ? b - 40 : 0;
    int hr = r < 206 ? r + 50 : 255, hg = g < 206 ? g + 50 : 255, hb = b < 206 ? b + 50 : 255;
    int sr = r > 49 ? r - 50 : 0, sg = g > 49 ? g - 50 : 0, sb = b > 49 ? b - 50 : 0;

    int x0 = *x, y0 = *y;

    DrawRectAlpha(x0 + 2, y0 + 2, w, h + 20, 40, 40, 55, 120);
    DrawRectAlpha(x0 + 5, y0 + 5, w, h + 20, 40, 40, 55, 70);

    DrawRect(x0, y0, w, h + 20, br, bg, bb);
    DrawRect(x0 + 2, y0 + 22, w - 4, h - 2, r, g, b);

    DrawRect(x0 + 2, y0 + 22, w - 4, 2, hr, hg, hb);
    DrawRect(x0 + 2, y0 + 22, 2, h - 4, hr, hg, hb);
    DrawRect(x0 + 2, y0 + h + 18, w - 4, 2, sr, sg, sb);
    DrawRect(x0 + w - 4, y0 + 22, 2, h - 2, sr, sg, sb);

    DrawRectGradient(x0 + 2, y0 + 2, w - 4, 18, 180, 160, 200, 120, 100, 150);

    DrawRect(x0 + 2, y0 + 2, w - 4, 2, hr, hg, hb);
    DrawRect(x0 + 2, y0 + 2, 2, 18, hr, hg, hb);
    DrawRect(x0 + 2, y0 + 18, w - 4, 2, sr, sg, sb);
    DrawRect(x0 + w - 4, y0 + 2, 2, 18, sr, sg, sb);

    return DrawX(x0 + w - 22, y0 + 2, 18, 18, 180, 40, 40, process_inst);
}
