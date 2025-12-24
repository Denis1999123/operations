#ifndef OPERATIONS_H_
#define OPERATIONS_H_

#include <regex.h>
#include <stddef.h>

typedef enum {
    OP_SUBSTITUTE = 1,
    OP_DELETE = 2,
} OperationType;

typedef struct {
    OperationType type;
    char* pattern;
    char* replacement;

    regex_t regex;
    int regex_ready;
} Command;

int ParseCommand(const char* cmd_text, Command* out, char* err, size_t err_cap);

int ApplyCommandToFile(const char* path, const Command* cmd, char* err, size_t err_cap);

void FreeCommand(Command* cmd);

#endif  // OPERATIONS_H_
