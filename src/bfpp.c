#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef enum { SRC_FILE, SRC_STRING } SourceType;

typedef struct Source {
    SourceType type;
    FILE *file;
    char *str;
    size_t pos;
    size_t len;
    struct Source *prev;
} Source;

typedef struct Macro {
    char *name;
    char *body;
    char *comment;
    char **params;
    int param_count;
    struct Macro *next;
} Macro;

typedef struct Guard {
    char *name;
    struct Guard *next;
} Guard;

Source *src_stack = NULL;
Macro *macros = NULL;
Guard *guards = NULL;

char *line_buf = NULL;
size_t lb_len = 0;
size_t lb_cap = 0;
int line_has_content = 0;

void emit_char(int c) {
    if (lb_cap == 0) {
        lb_cap = 128;
        line_buf = malloc(lb_cap);
    }
    if (c == '\n') {
        if (line_has_content) {
            fwrite(line_buf, 1, lb_len, stdout);
            putchar('\n');
        }
        lb_len = 0;
        line_has_content = 0;
    } else {
        if (lb_len + 1 >= lb_cap) {
            lb_cap *= 2;
            line_buf = realloc(line_buf, lb_cap);
        }
        line_buf[lb_len++] = c;
        if (!isspace(c)) {
            line_has_content = 1;
        }
    }
}

void emit_string(const char *s) {
    while (*s) emit_char(*s++);
}

void flush_output() {
    if (lb_len > 0 && line_has_content) {
        fwrite(line_buf, 1, lb_len, stdout);
        putchar('\n');
    }
    lb_len = 0;
    line_has_content = 0;
}

void push_file(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) { fprintf(stderr, "Error opening file: %s\n", filename); exit(1); }
    Source *s = malloc(sizeof(Source));
    s->type = SRC_FILE;
    s->file = f;
    s->prev = src_stack;
    src_stack = s;
}

void push_string(const char *str) {
    Source *s = malloc(sizeof(Source));
    s->type = SRC_STRING;
    s->str = strdup(str);
    s->pos = 0;
    s->len = strlen(str);
    s->prev = src_stack;
    src_stack = s;
}

void pop_source() {
    if (!src_stack) return;
    Source *s = src_stack;
    src_stack = s->prev;
    if (s->type == SRC_FILE) {
        if (s->file != stdin) fclose(s->file);
    } else free(s->str);
    free(s);
}

int get_char() {
    if (!src_stack) return EOF;
    int c = EOF;
    if (src_stack->type == SRC_FILE) {
        c = fgetc(src_stack->file);
    } else {
        if (src_stack->pos < src_stack->len) c = src_stack->str[src_stack->pos++];
        else c = EOF;
    }
    if (c == EOF) {
        pop_source();
        return get_char();
    }
    return c;
}

int peek_char() {
    if (!src_stack) return EOF;
    int c = EOF;
    if (src_stack->type == SRC_FILE) {
        c = fgetc(src_stack->file);
        if (c != EOF) ungetc(c, src_stack->file);
    } else {
        if (src_stack->pos < src_stack->len) c = src_stack->str[src_stack->pos];
    }
    return c;
}

void consume_until_brace() {
    while(1) {
        int c = get_char();
        if (c == '}' || c == EOF) break;
    }
}

void define_macro_base(const char *name, const char *body, char **params, int param_count) {
    Macro *m = macros;
    while (m) {
        if (strcmp(m->name, name) == 0) {
            if (m->body) free(m->body);
            m->body = strdup(body);
            if (m->params) {
                for(int i=0; i<m->param_count; i++) free(m->params[i]);
                free(m->params);
            }
            m->params = params;
            m->param_count = param_count;
            return;
        }
        m = m->next;
    }
    m = malloc(sizeof(Macro));
    m->name = strdup(name);
    m->body = strdup(body);
    m->comment = NULL;
    m->params = params;
    m->param_count = param_count;
    m->next = macros;
    macros = m;
}

void define_macro(const char *name, const char *body) {
    define_macro_base(name, body, NULL, 0);
}

void define_macro_comment(const char *name, const char *comment) {
    Macro *m = macros;
    while (m) {
        if (strcmp(m->name, name) == 0) {
            if (m->comment) free(m->comment);
            m->comment = strdup(comment);
            return;
        }
        m = m->next;
    }
    m = malloc(sizeof(Macro));
    m->name = strdup(name);
    m->body = NULL;
    m->comment = strdup(comment);
    m->params = NULL;
    m->param_count = 0;
    m->next = macros;
    macros = m;
}

void undef_macro(char *name) {
    Macro **curr = &macros;
    while (*curr) {
        if (strcmp((*curr)->name, name) == 0) {
            Macro *temp = *curr;
            *curr = (*curr)->next;
            free(temp->name);
            if (temp->body) free(temp->body);
            if (temp->comment) free(temp->comment);
            if (temp->params) {
                for(int i=0; i<temp->param_count; i++) free(temp->params[i]);
                free(temp->params);
            }
            free(temp);
            return;
        }
        curr = &(*curr)->next;
    }
}

Macro *find_macro(const char *name) {
    Macro *m = macros;
    while (m) {
        if (strcmp(m->name, name) == 0) return m;
        m = m->next;
    }
    return NULL;
}

void add_guard(const char *name) {
    Guard *g = malloc(sizeof(Guard));
    g->name = strdup(name);
    g->next = guards;
    guards = g;
}

int is_guarded(const char *name) {
    Guard *g = guards;
    while (g) {
        if (strcmp(g->name, name) == 0) return 1;
        g = g->next;
    }
    return 0;
}

char *read_word() {
    size_t cap = 16, len = 0;
    char *buf = malloc(cap);
    int c;
    while (isalnum(c = peek_char()) || c == '_') {
        get_char();
        if (len + 1 >= cap) buf = realloc(buf, cap *= 2);
        buf[len++] = c;
    }
    buf[len] = 0;
    return buf;
}

void skip_whitespace() {
    while (isspace(peek_char())) get_char();
}

char *read_definition_body() {
    skip_whitespace();
    size_t cap = 64, len = 0;
    char *buf = malloc(cap);
    int depth = 1;
    while (1) {
        int c = get_char();
        if (c == EOF) break;
        if (c == '{') depth++;
        if (c == '}') {
            depth--;
            if (depth == 0) break;
        }
        if (len + 2 >= cap) buf = realloc(buf, cap *= 2);
        buf[len++] = c;
    }
    buf[len] = 0;
    return buf;
}

char **read_decl_params(int *count) {
    skip_whitespace();
    if (peek_char() != '(') { *count = 0; return NULL; }
    get_char();
    
    char **params = NULL;
    int cap = 4;
    int cnt = 0;
    params = malloc(sizeof(char*) * cap);

    while (1) {
        skip_whitespace();
        if (peek_char() == ')') { get_char(); break; }
        
        char *pname = read_word();
        if (strlen(pname) > 0) {
            if (cnt >= cap) params = realloc(params, sizeof(char*) * (cap *= 2));
            params[cnt++] = pname;
        } else {
             free(pname);
        }

        skip_whitespace();
        if (peek_char() == ',') get_char();
        else if (peek_char() == ')') { get_char(); break; }
    }
    *count = cnt;
    return params;
}

char *trim_string(char *str) {
    char *end;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return strdup("");
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return strdup(str);
}

char **read_call_args(int expected_count) {
    skip_whitespace();
    if (peek_char() != '(') return NULL;
    get_char();

    char **args = malloc(sizeof(char*) * expected_count);
    for(int i=0; i<expected_count; i++) args[i] = NULL;
    
    int idx = 0;
    size_t cap = 64, len = 0;
    char *buf = malloc(cap);
    int depth_brace = 0;
    int depth_paren = 0;

    while (1) {
        int c = get_char();
        if (c == EOF) break;
        
        if (depth_brace == 0 && depth_paren == 0 && (c == ',' || c == ')')) {
            buf[len] = 0;
            if (idx < expected_count) {
                args[idx++] = trim_string(buf);
            }
            len = 0;
            if (c == ')') break;
        } else {
            if (c == '{') depth_brace++;
            else if (c == '}') { if(depth_brace > 0) depth_brace--; }
            else if (c == '(') depth_paren++;
            else if (c == ')') { if(depth_paren > 0) depth_paren--; }

            if (len + 2 >= cap) buf = realloc(buf, cap *= 2);
            buf[len++] = c;
        }
    }
    free(buf);
    return args;
}

int is_subst_word_char(char c) {
    return (isalnum(c) || c == '_') && c != 'x';
}

char *substitute_args(const char *body, char **param_names, char **args, int count) {
    size_t cap = strlen(body) * 2 + 1;
    if (cap < 64) cap = 64;
    size_t len = 0;
    char *res = malloc(cap);
    
    const char *p = body;
    while (*p) {
        if (is_subst_word_char(*p)) {
            const char *start = p;
            while (is_subst_word_char(*p)) p++;
            size_t wlen = p - start;
            char *word = malloc(wlen + 1);
            memcpy(word, start, wlen);
            word[wlen] = 0;
            
            int found_idx = -1;
            for (int i = 0; i < count; i++) {
                if (strcmp(word, param_names[i]) == 0) {
                    found_idx = i;
                    break;
                }
            }

            const char *sub = (found_idx != -1) ? args[found_idx] : word;
            size_t slen = strlen(sub);
            
            while (len + slen >= cap) res = realloc(res, cap *= 2);
            strcpy(res + len, sub);
            len += slen;
            free(word);
        } else {
            if (len + 1 >= cap) res = realloc(res, cap *= 2);
            res[len++] = *p++;
        }
    }
    res[len] = 0;
    return res;
}

char *read_path_until_brace() {
    skip_whitespace();
    size_t cap = 64, len = 0;
    char *buf = malloc(cap);
    int c;
    while ((c = peek_char()) != '}' && c != EOF) {
        get_char();
        if (len + 1 >= cap) {
            cap *= 2;
            buf = realloc(buf, cap);
        }
        buf[len++] = c;
    }
    buf[len] = '\0';
    while (len > 0 && isspace(buf[len - 1])) {
        buf[--len] = '\0';
    }
    return buf;
}

char *resolve_path(const char *arg) {
    if (arg[0] == '"') {
        size_t len = strlen(arg);
        if (len >= 2 && arg[len - 1] == '"') {
            char *path = strdup(arg + 1);
            path[len - 2] = '\0';
            return path;
        }
        return strdup(arg);
    }
    char *bfpp = getenv("BFPP");
    if (bfpp) {
        char *paths = strdup(bfpp);
        size_t len = strlen(paths);
        while (len > 0 && isspace((unsigned char)paths[len - 1])) paths[--len] = '\0';
        char *token = strtok(paths, ":");
        while (token) {
            size_t needed = strlen(token) + 1 + strlen(arg) + 1;
            char *full_path = malloc(needed);
            sprintf(full_path, "%s/%s", token, arg);
            FILE *f = fopen(full_path, "r");
            if (f) {
                fclose(f);
                free(paths);
                return full_path;
            }
            free(full_path);
            token = strtok(NULL, ":");
        }
        free(paths);
    }
    FILE *f = fopen(arg, "r");
    if (f) {
        fclose(f);
        return strdup(arg);
    }
    return NULL;
}

void print_sanitized_string(const char *str) {
    emit_string("/* ");
    while (*str) {
        if (strchr("+-<>.,[]", *str)) emit_char('?');
        else emit_char(*str);
        str++;
    }
    emit_string(" */");
}

char *find_macro_comment(const char *name) {
    Macro *m = find_macro(name);
    return m ? m->comment : NULL;
}

void print_macro_debug(const char *name) {
    Macro *m = find_macro(name);
    emit_string("/* expanded from "); emit_string(name); emit_string(" */");
}

char *expand_macros_once(const char *input) {
    size_t cap = strlen(input) * 2 + 1; 
    if (cap < 64) cap = 64;
    size_t len = 0;
    char *buf = malloc(cap);
    buf[0] = 0;

    const char *p = input;
    while (*p) {
        if (isalnum(*p) || *p == '_') {
            const char *start = p;
            while (isalnum(*p) || *p == '_') p++;
            size_t wlen = p - start;
            char *word = malloc(wlen + 1);
            memcpy(word, start, wlen);
            word[wlen] = 0;

            Macro *m = find_macro(word);
            const char *to_append = (m && m->body) ? m->body : word;
            size_t append_len = strlen(to_append);

            while (len + append_len >= cap) {
                cap *= 2;
                buf = realloc(buf, cap);
            }
            strcpy(buf + len, to_append);
            len += append_len;
            free(word);
        } else {
            if (len + 1 >= cap) {
                cap *= 2;
                buf = realloc(buf, cap);
            }
            buf[len++] = *p++;
            buf[len] = 0;
        }
    }
    return buf;
}

void process() {
    int c;
    while ((c = get_char()) != EOF) {
        if (c == '/' && peek_char() == '*') {
            get_char();
            while (1) {
                int n = get_char();
                if (n == EOF) break;
                if (n == '*' && peek_char() == '/') {
                    get_char();
                    break;
                }
            }
            continue;
        }

        if (c == '{') {
            skip_whitespace();
            int next = peek_char();
            if (strchr("LDU?Mx!CFS", next)) {
                if (next == 'L') {
                    read_word();
                    char *arg = read_path_until_brace();
                    consume_until_brace();
                    char *resolved = resolve_path(arg);
                    if (resolved) { push_file(resolved); free(resolved); }
                    else { fprintf(stderr, "Error: Could not find file '%s'\n", arg); exit(1); }
                    free(arg);
                } else if (next == 'S') {
                    read_word();
                    char *arg = read_path_until_brace();
                    if (is_guarded(arg)) {
                        free(arg);
                        pop_source();
                    } else {
                        add_guard(arg);
                        free(arg);
                        consume_until_brace();
                    }
                } else if (next == 'D') {
                    char *cmd = read_word();
                    skip_whitespace();
                    char *name = read_word();
                    char *body = read_definition_body();
                    define_macro(name, body);
                    free(cmd); free(name); free(body);
                } else if (next == 'F') {
                    char *cmd = read_word(); 
                    skip_whitespace();
                    char *name = read_word();
                    int pcount = 0;
                    char **params = read_decl_params(&pcount);
                    char *body = read_definition_body();
                    define_macro_base(name, body, params, pcount);
                    free(cmd); free(name); free(body);
                } else if (next == 'U') {
                    read_word(); skip_whitespace();
                    char *name = read_word();
                    undef_macro(name);
                    free(name);
                    consume_until_brace();
                } else if (next == '?') {
                    get_char(); skip_whitespace();
                    char *name = read_word();
                    print_macro_debug(name);
                    free(name);
                    consume_until_brace();
                } else if (next == 'M') {
                    get_char(); skip_whitespace();
                    char *name = read_word();
                    Macro *m = find_macro(name);
                    consume_until_brace();
                    if (m && m->body) push_string(m->body);
                    free(name);
                } else if (next == 'x') {
                    get_char();
                    skip_whitespace();
                    int count = 0;
                    while (isdigit(peek_char())) {
                        count = count * 10 + (get_char() - '0');
                    }
                    skip_whitespace();
                    if (get_char() != '{') { consume_until_brace(); continue; }
                    char *raw_body = read_definition_body(); 
                    consume_until_brace(); 
                    if (count > 0 && raw_body && *raw_body) {
                        char *expanded_body = expand_macros_once(raw_body);
                        size_t body_len = strlen(expanded_body);
                        char *final_str = malloc(body_len * count + 1);
                        if(final_str){
                            char *ptr = final_str;
                            for (int i = 0; i < count; i++) {
                                memcpy(ptr, expanded_body, body_len);
                                ptr += body_len;
                            }
                            *ptr = '\0';
                            push_string(final_str);
                            free(final_str);
                        }
                        free(expanded_body);
                    }
                    free(raw_body);
                } else if (next == '!') {
                    get_char(); skip_whitespace();
                    char *name = read_word();
                    char *comm = find_macro_comment(name);
                    if (comm) print_sanitized_string(comm);
                    else print_sanitized_string(name);
                    free(name);
                    consume_until_brace();
                } else if (next == 'C') {
                    char *cmd = read_word(); skip_whitespace();
                    char *name = read_word();
                    char *desc = read_definition_body();
                    define_macro_comment(name, desc);
                    free(cmd); free(name); free(desc);
                }
            } else { get_char(); } 
        }
        else if (c == '}') { } 
        else if (isalnum(c) || c == '_') {
            if (src_stack->type == SRC_FILE) ungetc(c, src_stack->file);
            else src_stack->pos--;
            char *word = read_word();
            Macro *m = find_macro(word);
            if (m) {
                if (m->param_count > 0) {
                    char **args = read_call_args(m->param_count);
                    if (args) {
                        char *expanded = substitute_args(m->body, m->params, args, m->param_count);
                        push_string(expanded);
                        free(expanded);
                        for(int i=0; i<m->param_count; i++) free(args[i]);
                        free(args);
                    } else {
                        emit_string(word); 
                    }
                } else if (m->body) {
                    push_string(m->body);
                }
            } else {
                emit_string(word);
            }
            free(word);
        } else {
            emit_char(c);
        }
    }
}

int main(int argc, char **argv) {
    if (argc > 1) {
        push_file(argv[1]);
    } else {
        Source *s = malloc(sizeof(Source));
        s->type = SRC_FILE;
        s->file = stdin;
        s->prev = NULL;
        src_stack = s;
    }
    process();
    flush_output();
    return 0;
}
