#include "baux2.h"
#include "../../include/serial.h"
#include "../../include/console.h"
#include "../../include/kheap.h"
#include "../../include/utils.h"
#include "../../include/gui.h"
#include "../../include/graphics.h"
#include "../../include/vesa.h"
#include "../../include/procsys.h"

static void df_print(const char *text) {
    printf("%s", text); // alt
}

baux2_print_t baux2_print_handler = df_print;

Value copy_value(Value value) {
    Value res = value;
    if (value.type == VAL_STRING) {
        res.string = strdup(value.string);
    }
    return res;
}

char* value_to_string(Value value) {
    char buf[256];
    switch (value.type) {
        case VAL_NUMBER:
            snprintf(buf, 256, "%g", value.number);
            return strdup(buf);
        case VAL_STRING:
            return strdup(value.string);
        case VAL_BOOL:
            return strdup(value.boolean ? "FLUFFY" : "FUZZY");
        case VAL_NIL:
            return strdup("nil");
        case VAL_FUNCTION:
            return strdup("<fn>");
        case VAL_NATIVE_FN:
            return strdup("<ntv_fn>");
        default:
            return strdup("unknown");
    }
}

void free_value(Value value) {
    if (value.type == VAL_STRING) {
        kfree(value.string);
    }
}

int g_baux2_curr_pid = 0;

void baux2_cleanup_process(Process* p) {
    if (p && p->data) {
        BAUx2_d* app_data = (BAUx2_d*)p->data;
        
        for (int i = 0; i < app_data->count; i++) {
            if (app_data->cmds[i].type == DRAW_TEXT && app_data->cmds[i].text) {
                kfree(app_data->cmds[i].text);
            }
        }
        
        if (app_data->root_comp) {
            PON_free(app_data->root_comp);
        }
        
        if (app_data->statements) {
            for (int i = 0; i < app_data->stmt_count; i++) {
                free_stmt(app_data->statements[i]);
            }
            kfree(app_data->statements);
        }
        
        if (app_data->env) {
            free_env(app_data->env);
        }

        if (app_data->source) {
            kfree(app_data->source);
        }

        kfree(p->data);
        p->data = NULL;
    }
}

void baux2_cleanup(Window* win) {
    if (!win) return;
    Process* p = get_process(win->pid);
    baux2_cleanup_process(p);
}

static void baux2_window_renderer(Window* win) {
    Process* p = get_process(win->pid);
    if (!p || !p->data) return;
    
    BAUx2_d* app_data = (BAUx2_d*)p->data;
    
    int cx = win->x + WIN_BORDER;
    int cy = win->y + TITLEBAR_H + WIN_BORDER;

    //primitives
    for (int i = 0; i < app_data->count; i++) {
        DrawCmd* cmd = &app_data->cmds[i];
        if (cmd->type == DRAW_RECT) {
            rect(cx + cmd->x, cy + cmd->y, cmd->w, cmd->h, cmd->color);
        } else if (cmd->type == DRAW_TEXT) {
            text(cmd->text, cx + cmd->x, cy + cmd->y, cmd->color, (font_t)cmd->font, FALSE);
        }
    }

    if (app_data->root_comp) {
        PON_render(app_data->root_comp, cx, cy);
    }
}

static void baux2_win_m_down(Window* win, int x, int y) {
    Process* p = get_process(win->pid);
    if (p && p->data) {
        BAUx2_d* app_data = (BAUx2_d*)p->data;
        if (app_data->root_comp) {
            int old_pid = g_baux2_curr_pid;
            g_baux2_curr_pid = win->pid;
            handle_mouse(app_data->root_comp, 0, 0, x, y, PON_MOUSE_DOWN);
            g_baux2_curr_pid = old_pid;
            is_dirty(TRUE);
        }
    }
}

static void baux2_win_m_up(Window* win, int x, int y) {
    Process* p = get_process(win->pid);
    if (p && p->data) {
        BAUx2_d* app_data = (BAUx2_d*)p->data;
        if (app_data->root_comp) {
            int old_pid = g_baux2_curr_pid;
            g_baux2_curr_pid = win->pid;
            handle_mouse(app_data->root_comp, 0, 0, x, y, PON_MOUSE_UP);
            g_baux2_curr_pid = old_pid;
            is_dirty(TRUE);
        }
    }
}

static void baux2_win_m_move(Window* win, int x, int y) {
    Process* p = get_process(win->pid);
    if (p && p->data) {
        BAUx2_d* app_data = (BAUx2_d*)p->data;
        if (app_data->root_comp) {
            int old_pid = g_baux2_curr_pid;
            g_baux2_curr_pid = win->pid;
            handle_mouse(app_data->root_comp, 0, 0, x, y, PON_MOUSE_MOVE);
            g_baux2_curr_pid = old_pid;
            is_dirty(TRUE);
        }
    }
}

static void baux2_win_on_key(Window* win, unsigned int key) {
    Process* p = get_process(win->pid);
    if (p && p->data) {
        BAUx2_d* app_data = (BAUx2_d*)p->data;
        if (app_data->root_comp) {
            int old_pid = g_baux2_curr_pid;
            g_baux2_curr_pid = win->pid;
            handle_key(app_data->root_comp, key);
            g_baux2_curr_pid = old_pid;
            is_dirty(TRUE);
        }
    }
}

Value call_baux2_function(Function *fn, int arg_count, Value *args) {
    Env *env = new_env(fn->closure);
    for (int i = 0; i < fn->param_count && i < arg_count; i++) {
        char name[64];
        int len = fn->params[i].length < 63 ? fn->params[i].length : 63;
        memcpy(name, fn->params[i].start, len);
        name[len] = '\0';
        env_define(env, name, args[i]);
    }

    for (int i = 0; i < fn->body_count; i++) {
        execute(fn->body[i], env);
        if (env->is_returning) break;
    }

    Value result = copy_value(env->return_value);
    free_env(env);
    return result;
}

static void baux2_pon_call_handler(PON_Comp* comp, int rx, int ry) {
    (void)rx; (void)ry;
    if (comp->appdata) {
        Value* callback = (Value*)comp->appdata;
        if (callback->type == VAL_FUNCTION) {
            call_baux2_function(callback->fn, 0, NULL);
        }
    }
}

static Value native_pon_rgb(int arg_count, Value *args) {
    if (arg_count < 3) return (Value){.type = VAL_NUMBER, .number = 0};
    uint8 r = (uint8)args[0].number;
    uint8 g = (uint8)args[1].number;
    uint8 b = (uint8)args[2].number;
    return (Value){.type = VAL_NUMBER, .number = (float)RGB(r, g, b)};
}

static Value native_pon_rect(int arg_count, Value *args) {
    if (arg_count < 5) return (Value){.type = VAL_NIL};
    
    Process* p = get_process(g_baux2_curr_pid);
    if (p && p->data) {
        BAUx2_d* app_data = (BAUx2_d*)p->data;
        if (app_data->count < MAX_DRAW_CMDS) {
            DrawCmd* cmd = &app_data->cmds[app_data->count++];
            cmd->type = DRAW_RECT;
            cmd->x = (int)args[0].number;
            cmd->y = (int)args[1].number;
            cmd->w = (int)args[2].number;
            cmd->h = (int)args[3].number;
            cmd->color = (uint32)args[4].number;
        } else {
            kprint("BAUx2 Warning: MAX_DRAW_CMDS reached!\n");
        }
    } else {
        // no process data, draw directly
        rect((int)args[0].number, (int)args[1].number, (int)args[2].number, (int)args[3].number, (uint32)args[4].number);
    }
    
    is_dirty(TRUE);
    return (Value){.type = VAL_NIL};
}

static Value native_pon_text(int arg_count, Value *args) {
    if (arg_count < 4) return (Value){.type = VAL_NIL};

    char* text_str = value_to_string(args[0]);
    int x = (int)args[1].number;
    int y = (int)args[2].number;
    uint32 color = (uint32)args[3].number;

    bool text_param_flag = false;
    if (arg_count >= 5 && args[4].type == VAL_BOOL) {
        text_param_flag = args[4].boolean;
    }

    int font_id = 1;

    uint32 bg_color = RGBA(172, 160, 188, 0);
    
    Process* p = get_process(g_baux2_curr_pid);
    if (p && p->data) {
        BAUx2_d* app_data = (BAUx2_d*)p->data;
        
        int found_bg = -1;
        int found_txt = -1;
        
        for (int i = 0; i < app_data->count; i++) {
            if (app_data->cmds[i].x == x && app_data->cmds[i].y == y) {
                if (app_data->cmds[i].type == DRAW_RECT) found_bg = i;
                else if (app_data->cmds[i].type == DRAW_TEXT) found_txt = i;
            }
        }

        const FontInfo* font = get_font_info((font_t)font_id);
        int char_w = (font ? font->width + 1 : 8);
        int char_h = (font ? font->height : 12);
        int text_w = strlen(text_str) * char_w;

        if (found_bg != -1) {
            app_data->cmds[found_bg].w = text_w;
            app_data->cmds[found_bg].h = char_h;
            app_data->cmds[found_bg].color = bg_color;
            app_data->cmds[found_bg].type = DRAW_RECT;
        } else if (app_data->count < MAX_DRAW_CMDS) {
            DrawCmd* bg_cmd = &app_data->cmds[app_data->count++];
            bg_cmd->type = DRAW_RECT;
            bg_cmd->x = x;
            bg_cmd->y = y;
            bg_cmd->w = text_w;
            bg_cmd->h = char_h;
            bg_cmd->color = bg_color;
        }

        if (found_txt != -1) {
            if (app_data->cmds[found_txt].text) kfree(app_data->cmds[found_txt].text);
            app_data->cmds[found_txt].text = text_str;
            app_data->cmds[found_txt].color = color;
            app_data->cmds[found_txt].font = font_id;
            app_data->cmds[found_txt].type = DRAW_TEXT;
        } else if (app_data->count < MAX_DRAW_CMDS) {
            DrawCmd* cmd = &app_data->cmds[app_data->count++];
            cmd->type = DRAW_TEXT;
            cmd->x = x;
            cmd->y = y;
            cmd->text = text_str;
            cmd->color = color;
            cmd->font = font_id;
        } else {
            kfree(text_str);
        }
    } else {
        const FontInfo* font = get_font_info((font_t)font_id);
        int char_w = (font ? font->width + 1 : 8);
        int char_h = (font ? font->height : 12);
        
        rect(x, y, strlen(text_str) * char_w, char_h, bg_color);
        text(text_str, x, y, color, (font_t)font_id, text_param_flag);
        kfree(text_str);
    }

    is_dirty(TRUE);
    return (Value){.type = VAL_NIL};
}

static Value native_pon_window(int arg_count, Value *args) {
    if (arg_count < 3) return (Value){.type = VAL_NIL};
    const char* title = args[0].string ? args[0].string : "Pebble (BAUx2)";
    int w = (int)args[1].number;
    int h = (int)args[2].number;
    Window* win = window_r(g_baux2_curr_pid, title, -1, -1, w, h, 172, 160, 188);
    if (win) {
        win->content_renderer = baux2_window_renderer;
        win->on_close = baux2_cleanup;
        win->on_mouse_down = baux2_win_m_down;
        win->on_mouse_up = baux2_win_m_up;
        win->on_mouse_move = baux2_win_m_move;
        win->on_key_press = baux2_win_on_key;
    }
    return (Value){.type = VAL_NUMBER, .number = (float)(win ? win->id : -1)};
}

static Value native_pon_button(int arg_count, Value *args) {
    if (arg_count < 5) return (Value){.type = VAL_NIL};
    
    Process* p = get_process(g_baux2_curr_pid);
    if (!p || !p->data) return (Value){.type = VAL_NIL};
    BAUx2_d* app_data = (BAUx2_d*)p->data;
    
    if (!app_data->root_comp) {
        app_data->root_comp = PANEL(0, 0, 1000, 1000, 0); //invis
        if (!app_data->root_comp) return (Value){.type = VAL_NIL};
        app_data->root_comp->draw = NULL; 
    }
    
    const char* label = args[0].string;
    int x = (int)args[1].number;
    int y = (int)args[2].number;
    int w = (int)args[3].number;
    int h = (int)args[4].number;
    
    PON_Comp* btn = BUTTON(x, y, w, h, label, baux2_pon_call_handler);
    if (!btn) return (Value){.type = VAL_NIL};

    if (arg_count > 5 && args[5].type == VAL_FUNCTION) {
        btn->appdata = kmalloc(sizeof(Value));
        if (btn->appdata) {
            *((Value*)btn->appdata) = copy_value(args[5]);
        }
    }
    
    PON_child(app_data->root_comp, btn);
    is_dirty(TRUE);
    return (Value){.type = VAL_NUMBER, .number = (float)(uint32)btn};
}

static Value native_pon_text_input(int arg_count, Value *args) {
    if (arg_count < 5) return (Value){.type = VAL_NIL};
    
    Process* p = get_process(g_baux2_curr_pid);
    if (!p || !p->data) return (Value){.type = VAL_NIL};
    BAUx2_d* app_data = (BAUx2_d*)p->data;
    
    if (!app_data->root_comp) {
        app_data->root_comp = PANEL(0, 0, 1000, 1000, 0);
        if (!app_data->root_comp) return (Value){.type = VAL_NIL};
        app_data->root_comp->draw = NULL;
    }
    
    int x = (int)args[0].number;
    int y = (int)args[1].number;
    int w = (int)args[2].number;
    int h = (int)args[3].number;
    int max_len = (int)args[4].number;
    
    PON_Comp* input = TEXTFIELD(x, y, w, h, max_len);
    if (input) {
        if (arg_count > 5 && args[5].type == VAL_FUNCTION) {
            input->appdata = kmalloc(sizeof(Value));
            if (input->appdata) {
                *((Value*)input->appdata) = copy_value(args[5]);
            }
            input->on_change = baux2_pon_call_handler;
        }
        PON_child(app_data->root_comp, input);
    }
    
    is_dirty(TRUE);
    return (Value){.type = VAL_NUMBER, .number = (float)(uint32)input};
}

static Value native_pon_read(int arg_count, Value *args) {
    if (arg_count < 1 || args[0].type != VAL_NUMBER) return (Value){.type = VAL_NIL};

    PON_Comp* comp = (PON_Comp*)(uint32)args[0].number;
    if (!comp) return (Value){.type = VAL_NIL};

    if (comp->type == COMP_TEXTFIELD) {
        PON_TextField_d* data = (PON_TextField_d*)comp->data;
        if (data && data->buffer) {
            return (Value){.type = VAL_STRING, .string = strdup(data->buffer)};
        }
    } else if (comp->type == COMP_TEXT) {
        PON_Text_d* data = (PON_Text_d*)comp->data;
        if (data && data->str) {
            return (Value){.type = VAL_STRING, .string = strdup(data->str)};
        }
    } else if (comp->type == COMP_BUTTON) {
        PON_Button_d* data = (PON_Button_d*)comp->data;
        if (data && data->label) {
            return (Value){.type = VAL_STRING, .string = strdup(data->label)};
        }
    }

    return (Value){.type = VAL_NIL};
}

static void register_pon_api(Env *env) {
    env_define(env, "PON.rgb", (Value){.type = VAL_NATIVE_FN, .native = native_pon_rgb});
    env_define(env, "PON.rect", (Value){.type = VAL_NATIVE_FN, .native = native_pon_rect});
    env_define(env, "PON.text", (Value){.type = VAL_NATIVE_FN, .native = native_pon_text});
    env_define(env, "PON.window", (Value){.type = VAL_NATIVE_FN, .native = native_pon_window});
    env_define(env, "PON.button", (Value){.type = VAL_NATIVE_FN, .native = native_pon_button});
    env_define(env, "PON.field", (Value){.type = VAL_NATIVE_FN, .native = native_pon_text_input});
    env_define(env, "PON.text_input", (Value){.type = VAL_NATIVE_FN, .native = native_pon_text_input});
    env_define(env, "PON.read", (Value){.type = VAL_NATIVE_FN, .native = native_pon_read});
}

Value eval_expr(Expr *expr, Env *env) {
  Value val;
  val.type = VAL_NIL;

  if (expr == NULL) return val;

  switch (expr->type) {
    case EXPR_LITERAL:
      switch (expr->literal.lit_type) {
        case LIT_NIL:    val.type = VAL_NIL; break;
        case LIT_STRING: val.type = VAL_STRING; val.string = strdup(expr->literal.string); break;
        case LIT_BOOL:   val.type = VAL_BOOL;   val.boolean = expr->literal.boolean; break;
        case LIT_NUMBER: val.type = VAL_NUMBER; val.number = expr->literal.number; break;
      }
      return val;

    case EXPR_VAR:
      if (!env_get(env, expr->var.name, &val)) {
        char err[128];
        snprintf(err, 128, "err: Undefined variable '%.*s'.\n", 
               expr->var.name.length, expr->var.name.start);
        baux2_print_handler(err);
      }
      return val;

    case EXPR_ASSIGN:
      val = eval_expr(expr->assign.value, env);
      if (!env_assign(env, expr->assign.name, val)) {
        char err[128];
        snprintf(err, 128, "err: Undefined variable '%.*s'.\n", 
               expr->assign.name.length, expr->assign.name.start);
        baux2_print_handler(err);
      }
      return val;

    case EXPR_BINARY: {
      Value left = eval_expr(expr->binary.left, env);
      Value right = eval_expr(expr->binary.right, env);
      switch (expr->binary.op.type) {
        case TOKEN_PLUS:
          if (left.type == VAL_NUMBER && right.type == VAL_NUMBER) {
            val.type = VAL_NUMBER;
            val.number = left.number + right.number;
          } else if (left.type == VAL_STRING || right.type == VAL_STRING) {
            char* s1 = value_to_string(left);
            char* s2 = value_to_string(right);
            val.type = VAL_STRING;
            int len = strlen(s1) + strlen(s2);
            val.string = (char *)kmalloc(len + 1);
            strcpy(val.string, s1);
            strcat(val.string, s2);
            kfree(s1);
            kfree(s2);
          } else {
            baux2_print_handler("err: Operands for '+' must be numbers or at least one must be a string.\n");
          }
          break;
        case TOKEN_MINUS:
          if (left.type == VAL_NUMBER && right.type == VAL_NUMBER) {
            val.type = VAL_NUMBER; val.number = left.number - right.number;
          } else {
            baux2_print_handler("err: Operands for '-' must be numbers.\n");
          }
          break;
        case TOKEN_STAR:
          if (left.type == VAL_NUMBER && right.type == VAL_NUMBER) {
            val.type = VAL_NUMBER; val.number = left.number * right.number;
          } else {
            baux2_print_handler("err: Operands for '*' must be numbers.\n");
          }
          break;
        case TOKEN_SLASH:
          if (left.type == VAL_NUMBER && right.type == VAL_NUMBER) {
            if (right.number == 0) {
              baux2_print_handler("err: Division by zero.\n");
              val.type = VAL_NUMBER; val.number = 0;
            } else {
              val.type = VAL_NUMBER; val.number = left.number / right.number;
            }
          } else {
            baux2_print_handler("err: Operands for '/' must be numbers.\n");
          }
          break;
        case TOKEN_GREATER:
          if (left.type == VAL_NUMBER && right.type == VAL_NUMBER) {
            val.type = VAL_BOOL; val.boolean = left.number > right.number;
          } else {
            baux2_print_handler("err: Operands for '>' must be numbers.\n");
          }
          break;
        case TOKEN_GREATER_EQUAL:
          if (left.type == VAL_NUMBER && right.type == VAL_NUMBER) {
            val.type = VAL_BOOL; val.boolean = left.number >= right.number;
          } else {
            baux2_print_handler("err: Operands for '>=' must be numbers.\n");
          }
          break;
        case TOKEN_LESS:
          if (left.type == VAL_NUMBER && right.type == VAL_NUMBER) {
            val.type = VAL_BOOL; val.boolean = left.number < right.number;
          } else {
            baux2_print_handler("err: Operands for '<' must be numbers.\n");
          }
          break;
        case TOKEN_LESS_EQUAL:
          if (left.type == VAL_NUMBER && right.type == VAL_NUMBER) {
            val.type = VAL_BOOL; val.boolean = left.number <= right.number;
          } else {
            baux2_print_handler("err: Operands for '<=' must be numbers.\n");
          }
          break;
        case TOKEN_BANG_EQUAL:
          val.type = VAL_BOOL;
          if (left.type != right.type) val.boolean = true;
          else if (left.type == VAL_NUMBER) val.boolean = left.number != right.number;
          else if (left.type == VAL_BOOL) val.boolean = left.boolean != right.boolean;
          else if (left.type == VAL_STRING) val.boolean = strcmp(left.string, right.string) != 0;
          break;
        case TOKEN_EQUAL_EQUAL:
          val.type = VAL_BOOL;
          if (left.type != right.type) val.boolean = false;
          else if (left.type == VAL_NUMBER) val.boolean = left.number == right.number;
          else if (left.type == VAL_BOOL) val.boolean = left.boolean == right.boolean;
          else if (left.type == VAL_STRING) val.boolean = strcmp(left.string, right.string) == 0;
          break;
        default: break;
      }
      free_value(left);
      free_value(right);
      return val;
    }

    case EXPR_UNARY: {
        Value right = eval_expr(expr->unary.right, env);
        if (expr->unary.op.type == TOKEN_MINUS) {
        if (right.type == VAL_NUMBER) {
            val.type = VAL_NUMBER;
            val.number = -right.number;
        } else {
            baux2_print_handler("err: Operand for '-' must be a number.\n");
        }
        } else if (expr->unary.op.type == TOKEN_BANG) {
        val.type = VAL_BOOL;
        val.boolean = (right.type == VAL_NIL || (right.type == VAL_BOOL && !right.boolean));
        }
        free_value(right);
        return val;
    }

    case EXPR_CALL: {
        Value callee = eval_expr(expr->call.callee, env);
        Value *args = (Value *)kmalloc(sizeof(Value) * expr->call.argc);
        for (int i = 0; i < expr->call.argc; i++) {
        args[i] = eval_expr(expr->call.args[i], env);
        }

        Value result;
        result.type = VAL_NIL;

        if (callee.type == VAL_NATIVE_FN) {
            result = callee.native(expr->call.argc, args);
        } else if (callee.type == VAL_FUNCTION) {
            result = call_baux2_function(callee.fn, expr->call.argc, args);
        } else {
            baux2_print_handler("err: Can only call functions.\n");
        }

        for (int i = 0; i < expr->call.argc; i++) free_value(args[i]);
        kfree(args);
        free_value(callee);
        return result;
    }

    default: return val;
  }
}

void execute(Stmt *stmt, Env *env) {
  if (env->is_returning) return;

  switch (stmt->type) {
    case STMT_PRINT: {
      Value val = eval_expr(stmt->print.expression, env);
      char buf[256];
      if (val.type == VAL_NUMBER) snprintf(buf, 256, "%d\n", (int)val.number);
      else if (val.type == VAL_STRING) snprintf(buf, 256, "%s\n", val.string);
      else if (val.type == VAL_BOOL) snprintf(buf, 256, "%s\n", val.boolean ? "FLUFFY" : "FUZZY");
      else if (val.type == VAL_NIL) snprintf(buf, 256, "nil\n");
      
      baux2_print_handler(buf);
      free_value(val);
      break;
    }
    case STMT_VAR: {
      Value val;
      val.type = VAL_NIL;
      if (stmt->var.initializer != NULL) {
        val = eval_expr(stmt->var.initializer, env);
      }
      char name[64];
      int len = stmt->var.name.length < 63 ? stmt->var.name.length : 63;
      memcpy(name, stmt->var.name.start, len);
      name[len] = '\0';
      env_define(env, name, val);
      free_value(val);
      break;
    }
    case STMT_BLOCK: {
      Env *new_scope = new_env(env);
      for (int i = 0; i < stmt->block.count; i++) {
        execute(stmt->block.statements[i], new_scope);
        if (new_scope->is_returning) {
          env->is_returning = true;
          env->return_value = copy_value(new_scope->return_value);
          break;
        }
      }
      free_env(new_scope);
      break;
    }
    case STMT_IF: {
      Value cond = eval_expr(stmt->if_stmt.condition, env);
      bool is_true = !(cond.type == VAL_NIL || (cond.type == VAL_BOOL && !cond.boolean) || (cond.type == VAL_NUMBER && cond.number == 0));
      if (is_true) {
        execute(stmt->if_stmt.then_branch, env);
      } else if (stmt->if_stmt.else_branch != NULL) {
        execute(stmt->if_stmt.else_branch, env);
      }
      free_value(cond);
      break;
    }
    case STMT_WHILE: {
      int iterations = 0;
      const int MAX_ITERATIONS = 10000;
      for (;;) {
        if (iterations++ > MAX_ITERATIONS) {
            baux2_print_handler("err: Loop exceeded maximum iterations (10000).\n");
            break;
        }
        Value cond = eval_expr(stmt->while_stmt.condition, env);
        bool is_true = !(cond.type == VAL_NIL || (cond.type == VAL_BOOL && !cond.boolean) || (cond.type == VAL_NUMBER && cond.number == 0));
        if (!is_true) {
          free_value(cond);
          break;
        }
        execute(stmt->while_stmt.body, env);
        free_value(cond);
        if (env->is_returning) break;
      }
      break;
    }
    case STMT_FOR_RANGE: {
      Value val = eval_expr(stmt->for_range.count_expr, env);
      if (val.type != VAL_NUMBER) {
          baux2_print_handler("err: PONDE loop count must be a number.\n");
          free_value(val);
          break;
      }
      int count = (int)val.number;
      if (count > 10000) count = 10000;
      for (int i = 0; i < count; i++) {
          execute(stmt->for_range.body, env);
          if (env->is_returning) break;
      }
      free_value(val);
      break;
    }
    case STMT_FUNCTION: {
      Function *fn = (Function *)kmalloc(sizeof(Function));
      fn->name = stmt->function.name;
      fn->params = stmt->function.params;
      fn->param_count = stmt->function.param_count;
      fn->body = stmt->function.body;
      fn->body_count = stmt->function.body_count;
      fn->closure = env;
      
      Value val;
      val.type = VAL_FUNCTION;
      val.fn = fn;
      
      char name[64];
      int len = fn->name.length < 63 ? fn->name.length : 63;
      memcpy(name, fn->name.start, len);
      name[len] = '\0';
      env_define(env, name, val);
      break;
    }
    case STMT_RETURN: {
      Value value = (Value){.type = VAL_NIL};
      if (stmt->return_stmt.value != NULL) {
        value = eval_expr(stmt->return_stmt.value, env);
      }
      env->is_returning = true;
      env->return_value = value;
      break;
    }
    case STMT_PANIC: {
      Value val = eval_expr(stmt->panic_stmt.message, env);
      char *msg = "BAUx2 Panic!";
      if (val.type == VAL_STRING) msg = val.string;
      panic(msg);
      free_value(val);
      break;
    }
    case STMT_EXPR: {
      Value val = eval_expr(stmt->expr.expression, env);
      free_value(val);
      break;
    }
    case STMT_IMPORT: {
      char name[64];
      int len = stmt->import_stmt.name.length < 63 ? stmt->import_stmt.name.length : 63;
      memcpy(name, stmt->import_stmt.name.start, len);
      name[len] = '\0';
      
      if (strcmp(name, "PON") == 0) {
        register_pon_api(env);
      } else {
        char err[128];
        snprintf(err, 128, "err: Unknown library '%s'.\n", name);
        baux2_print_handler(err);
      }
      break;
    }
    default: break;
  }
}

Env *interpret(Stmt **statements, int count) {
    Env *globals = new_env(NULL);
    for (int i = 0; i < count; i++) {
        execute(statements[i], globals);
    }
    return globals;
}