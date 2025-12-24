#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "operations.h"

static void PrintUsage(const char* prog) {
    fprintf(stderr,
            "Usage: %s <input_file> '<command>'\n"
            "Commands (sed-like):\n"
            "  s/regex/replacement/   - replace ALL matches in each line\n"
            "  /regex/d               - delete lines matching regex\n"
            "Examples:\n"
            "  %s input.txt 's/old/new/'\n"
            "  %s input.txt '/^DEBUG/d'\n"
            "  %s input.txt 's/^/[PFX] /'\n"
            "  %s input.txt 's/$/ [SFX]/'\n",
            prog, prog, prog, prog, prog);
}

int main(int argc, char** argv) {
    if (argc != 3) {
        PrintUsage(argv[0]);
        return 2;
    }

    const char* path = argv[1];
    const char* cmd_text = argv[2];

    char err[512];
    Command cmd;
    memset(&cmd, 0, sizeof(cmd));

    if (ParseCommand(cmd_text, &cmd, err, sizeof(err)) != 0) {
        fprintf(stderr, "Command error: %s\n", err);
        FreeCommand(&cmd);
        return 3;
    }

    if (ApplyCommandToFile(path, &cmd, err, sizeof(err)) != 0) {
        fprintf(stderr, "File error: %s\n", err);
        FreeCommand(&cmd);
        return 4;
    }

    FreeCommand(&cmd);
    return 0;
}
