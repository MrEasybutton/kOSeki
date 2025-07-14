#ifndef PROCESS_SYSTEM_H
#define PROCESS_SYSTEM_H

#define process_type_void 0
#define process_type_string_buffer 1
#define procparamlen 10
#define MAX_PROCESSES 256

struct Process {
    int priority;
    int process_inst;
    char ca1[100];
    int i1;

    int (*function)(int);
};

extern struct Process process[256];
extern int iparams[100];
extern int ProcessLen;

void CloseProcess(int process_inst);
int NullProcess(int process_inst);

#endif 
