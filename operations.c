#define _POSIX_C_SOURCE 200809L

#include "operations.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
    char* data;
    size_t len;
    size_t cap;
} StrBuf;

static int SbEnsure(StrBuf* b, size_t need_cap) {
    if (need_cap <= b->cap) return 0;
    size_t new_cap = b->cap ? b->cap : 64;
    while (new_cap < need_cap) new_cap *= 2;
    char* p = (char*)realloc(b->data, new_cap);
    if (!p) return -1;
    b->data = p;
    b->cap = new_cap;
    return 0;
}

static int SbAppendN(StrBuf* b, const char* s, size_t n) {
    if (SbEnsure(b, b->len + n + 1) != 0) return -1;
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
    return 0;
}

static int SbAppend(StrBuf* b, const char* s) {
    return SbAppendN(b, s, strlen(s));
}

static void CmdReset(Command* c) {
    memset(c, 0, sizeof(*c));
}

static void SetErr(char* err, size_t err_cap, const char* msg) {
    if (err && err_cap) {
        snprintf(err, err_cap, "%s", msg);
    }
}

static int ParseUntilDelim(const char* s, size_t* io_i, char delim, char** out,
                           char* err, size_t err_cap) {
    StrBuf b = {0};
    size_t i = *io_i;

    while (s[i] != '\0') {
        if (s[i] == '\\') {
            char next = s[i + 1];
            if (next == '\0') {
                if (SbAppendN(&b, "\\", 1) != 0) {
                    free(b.data);
                    SetErr(err, err_cap, "out of memory");
                    return 1;
                }
                i++;
                continue;
            }
            if (next == delim || next == '\\') {
                if (SbAppendN(&b, &next, 1) != 0) {
                    free(b.data);
                    SetErr(err, err_cap, "out of memory");
                    return 1;
                }
                i += 2;
                continue;
            }
            if (SbAppendN(&b, "\\", 1) != 0) {
                free(b.data);
                SetErr(err, err_cap, "out of memory");
                return 1;
            }
            i++;
            continue;
        }

        if (s[i] == delim) {
            i++;
            *io_i = i;
            *out = b.data ? b.data : strdup("");
            if (!*out) {
                free(b.data);
                SetErr(err, err_cap, "out of memory");
                return 1;
            }
            return 0;
        }

        if (SbAppendN(&b, &s[i], 1) != 0) {
            free(b.data);
            SetErr(err, err_cap, "out of memory");
            return 1;
        }
        i++;
    }

    free(b.data);
    SetErr(err, err_cap, "unexpected end of command (missing '/')");
    return 1;
}

static int CompileRegex(Command* out, char* err, size_t err_cap) {
    int rc = regcomp(&out->regex, out->pattern, REG_EXTENDED);
    if (rc != 0) {
        char buf[256];
        regerror(rc, &out->regex, buf, sizeof(buf));
        snprintf(err, err_cap, "regex compile failed: %s", buf);
        return 1;
    }
    out->regex_ready = 1;
    return 0;
}

int ParseCommand(const char* cmd_text, Command* out, char* err, size_t err_cap) {
    if (!cmd_text || !out) {
        SetErr(err, err_cap, "internal: null argument");
        return 1;
    }

    CmdReset(out);

    if (cmd_text[0] == 's' && cmd_text[1] == '/') {
        out->type = OP_SUBSTITUTE;
        size_t i = 2;
        if (ParseUntilDelim(cmd_text, &i, '/', &out->pattern, err, err_cap) != 0) {
            FreeCommand(out);
            return 1;
        }
        if (!out->pattern || out->pattern[0] == '\0') {
            FreeCommand(out);
            SetErr(err, err_cap, "empty regex is not allowed");
            return 1;
        }
        if (ParseUntilDelim(cmd_text, &i, '/', &out->replacement, err, err_cap) != 0) {
            FreeCommand(out);
            return 1;
        }
        if (cmd_text[i] != '\0') {
            FreeCommand(out);
            SetErr(err, err_cap, "unexpected tail after command (expected end) ");
            return 1;
        }
        if (CompileRegex(out, err, err_cap) != 0) {
            FreeCommand(out);
            return 1;
        }
        return 0;
    }

    if (cmd_text[0] == '/') {
        out->type = OP_DELETE;
        size_t i = 1;
        if (ParseUntilDelim(cmd_text, &i, '/', &out->pattern, err, err_cap) != 0) {
            FreeCommand(out);
            return 1;
        }
        if (!out->pattern || out->pattern[0] == '\0') {
            FreeCommand(out);
            SetErr(err, err_cap, "empty regex is not allowed");
            return 1;
        }
        if (cmd_text[i] != 'd' || cmd_text[i + 1] != '\0') {
            FreeCommand(out);
            SetErr(err, err_cap, "delete command must look like /regex/d");
            return 1;
        }
        if (CompileRegex(out, err, err_cap) != 0) {
            FreeCommand(out);
            return 1;
        }
        return 0;
    }

    SetErr(err, err_cap, "unsupported command format");
    return 1;
}

static char* ReplaceAllRegex(const char* src, const regex_t* re, const char* replacement,
                             char* err, size_t err_cap) {
    StrBuf out = {0};
    size_t offset = 0;
    size_t src_len = strlen(src);

    if (SbEnsure(&out, src_len + 32) != 0) {
        SetErr(err, err_cap, "out of memory");
        return NULL;
    }

    while (offset <= src_len) {
        const char* cur = src + offset;
        regmatch_t m;
        int eflags = 0;
        if (offset > 0) eflags |= REG_NOTBOL;
        if (offset < src_len) eflags |= REG_NOTEOL;

        int rc = regexec(re, cur, 1, &m, eflags);
        if (rc != 0) {
            if (SbAppend(&out, cur) != 0) {
                free(out.data);
                SetErr(err, err_cap, "out of memory");
                return NULL;
            }
            break;
        }

        size_t so = (size_t)m.rm_so;
        size_t eo = (size_t)m.rm_eo;

        if (SbAppendN(&out, cur, so) != 0) {
            free(out.data);
            SetErr(err, err_cap, "out of memory");
            return NULL;
        }
        if (SbAppend(&out, replacement) != 0) {
            free(out.data);
            SetErr(err, err_cap, "out of memory");
            return NULL;
        }

        offset += eo;

        if (eo == so) {
            if (src[offset] == '\0') {
                break;
            }
            if (SbAppendN(&out, &src[offset], 1) != 0) {
                free(out.data);
                SetErr(err, err_cap, "out of memory");
                return NULL;
            }
            offset += 1;
        }
    }

    return out.data ? out.data : strdup("");
}

static int LineMatches(const char* line, const regex_t* re) {
    return regexec(re, line, 0, NULL, 0) == 0;
}

static int MakeTempPathNear(const char* path, char* out, size_t out_cap) {
    if (!path || !out || out_cap == 0) return 1;
    int n = snprintf(out, out_cap, "%s.tmpXXXXXX", path);
    if (n < 0 || (size_t)n >= out_cap) return 1;
    return 0;
}

int ApplyCommandToFile(const char* path, const Command* cmd, char* err, size_t err_cap) {
    if (!path || !cmd || !cmd->regex_ready) {
        SetErr(err, err_cap, "internal: invalid arguments");
        return 1;
    }

    struct stat st;
    int have_stat = (stat(path, &st) == 0);

    FILE* in = fopen(path, "r");
    if (!in) {
        snprintf(err, err_cap, "cannot open input file: %s", strerror(errno));
        return 1;
    }

    char tmp_path[PATH_MAX];
    if (MakeTempPathNear(path, tmp_path, sizeof(tmp_path)) != 0) {
        fclose(in);
        SetErr(err, err_cap, "path too long");
        return 1;
    }

    int tmp_fd = mkstemp(tmp_path);
    if (tmp_fd < 0) {
        fclose(in);
        snprintf(err, err_cap, "cannot create temp file: %s", strerror(errno));
        return 1;
    }

    if (have_stat) {
        (void)fchmod(tmp_fd, st.st_mode);
    }

    FILE* out = fdopen(tmp_fd, "w");
    if (!out) {
        close(tmp_fd);
        unlink(tmp_path);
        fclose(in);
        snprintf(err, err_cap, "fdopen failed: %s", strerror(errno));
        return 1;
    }

    char* line = NULL;
    size_t cap = 0;
    ssize_t nread;

    while ((nread = getline(&line, &cap, in)) != -1) {
        int had_nl = 0;
        if (nread > 0 && line[nread - 1] == '\n') {
            had_nl = 1;
            line[nread - 1] = '\0';
        }

        if (cmd->type == OP_DELETE) {
            if (LineMatches(line, &cmd->regex)) {
                continue;
            }
            fputs(line, out);
            if (had_nl) fputc('\n', out);
            continue;
        }

        if (cmd->type == OP_SUBSTITUTE) {
            char local_err[256];
            local_err[0] = '\0';
            char* replaced = ReplaceAllRegex(line, &cmd->regex, cmd->replacement,
                                             local_err, sizeof(local_err));
            if (!replaced) {
                free(line);
                fclose(in);
                fclose(out);
                unlink(tmp_path);
                snprintf(err, err_cap, "replace failed: %s",
                         local_err[0] ? local_err : "unknown");
                return 1;
            }
            fputs(replaced, out);
            if (had_nl) fputc('\n', out);
            free(replaced);
            continue;
        }

        fputs(line, out);
        if (had_nl) fputc('\n', out);
    }

    free(line);

    if (ferror(in)) {
        fclose(in);
        fclose(out);
        unlink(tmp_path);
        snprintf(err, err_cap, "read failed: %s", strerror(errno));
        return 1;
    }

    if (fflush(out) != 0) {
        fclose(in);
        fclose(out);
        unlink(tmp_path);
        snprintf(err, err_cap, "write flush failed: %s", strerror(errno));
        return 1;
    }

    fclose(in);
    if (fclose(out) != 0) {
        unlink(tmp_path);
        snprintf(err, err_cap, "write close failed: %s", strerror(errno));
        return 1;
    }

    if (rename(tmp_path, path) != 0) {
        unlink(tmp_path);
        snprintf(err, err_cap, "rename failed: %s", strerror(errno));
        return 1;
    }

    return 0;
}

void FreeCommand(Command* cmd) {
    if (!cmd) return;
    if (cmd->regex_ready) {
        regfree(&cmd->regex);
        cmd->regex_ready = 0;
    }
    free(cmd->pattern);
    free(cmd->replacement);
    cmd->pattern = NULL;
    cmd->replacement = NULL;
}
