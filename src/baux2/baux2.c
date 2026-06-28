#include "baux2.h"
#include "../../include/kheap.h"
#include "../../include/procsys.h"

extern int g_baux2_curr_pid;

void baux2_run(const char *source) {
    Process* p = get_process(g_baux2_curr_pid);

    if (p && p->data) {
        BAUx2_d* app_data = (BAUx2_d*)p->data;
        app_data->source = (char*)source;
    }

    int count = 0;
    Stmt** statements = parse(source, &count);

    if (statements != NULL) {
        Env* globals = interpret(statements, count);

        if (p && p->data) {
            BAUx2_d* app_data = (BAUx2_d*)p->data;

            app_data->statements = statements;
            app_data->stmt_count = count;
            app_data->env = globals;
        } else {
            // fallback
            for (int i = 0; i < count; i++) {
                free_stmt(statements[i]);
            }

            kfree(statements);

            if (globals != NULL) {
                free_env(globals);
            }
        }
    }
}