#ifndef REAPER_H
#define REAPER_H

#include "types.h"

void launch_reaper();
void reaper_check_redirect(void);
void reaper_process_deferred_tasks(void);

#endif
