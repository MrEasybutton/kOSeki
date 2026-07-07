#include "baux2.h"
#include "../../include/kheap.h"

Env *new_env(Env *enclosing) {
    Env *env = (Env *)kmalloc(sizeof(Env));
    env->enclosing = enclosing;
    env->count = 0;
    env->capacity = 8;
    env->entries = (EnvEntry *)kmalloc(sizeof(EnvEntry) * env->capacity);
    env->is_returning = false;
    env->return_value = (Value){.type = VAL_NIL};
    return env;
}

void env_define(Env *env, const char *name, Value value) {
    if (env->count >= env->capacity) {
        env->capacity *= 2;
        env->entries = (EnvEntry *)krealloc(env->entries, sizeof(EnvEntry) * env->capacity);
    }

    env->entries[env->count].name = strdup(name);
    env->entries[env->count].value = copy_value(value);
    // char dbg[128]; snprintf(dbg, 128, "DEBUG: Defined '%s' type: %d val: %f\n", name, value.type, value.number);
    // baux2_print_handler(dbg);
    env->count++;
}

bool env_get(Env *env, Token name, Value *value) {
    for (int i = 0; i < env->count; i++) {
        if (strlen(env->entries[i].name) == name.length &&
            memcmp(env->entries[i].name, name.start, name.length) == 0)
        {
            *value = copy_value(env->entries[i].value);
            // char dbg[128]; snprintf(dbg, 128, "DEBUG: Found '%.*s' type: %d val: %f\n", name.length, name.start, value->type, value->number);
            // baux2_print_handler(dbg);
            return true;
        }
    }

    if (env->enclosing != NULL) return env_get(env->enclosing, name, value);

    return false;
}

bool env_assign(Env *env, Token name, Value value)
{
    for (int i = 0; i < env->count; i++) {
        if (strlen(env->entries[i].name) == name.length && memcmp(env->entries[i].name, name.start, name.length) == 0) {
            free_value(env->entries[i].value);
            env->entries[i].value = copy_value(value);
            return true;
        }
    }

    if (env->enclosing != NULL) return env_assign(env->enclosing, name, value);

    return false;
}

void free_env(Env *env) {
    for (int i = 0; i < env->count; i++) {
        kfree(env->entries[i].name);
        free_value(env->entries[i].value);
    }
    kfree(env->entries);
    kfree(env);
}