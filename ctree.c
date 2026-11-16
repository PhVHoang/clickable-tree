#define _POSIX_C_SOURCE 200809L
#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#endif
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fnmatch.h>
#include <fcntl.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>
#include <wchar.h>

typedef struct {
    bool all, dirs_only, full_path, permissions, sizes, human, time_sort;
    bool reverse, dirs_first, no_indent, classify, no_report, plain, emit_cd;
    bool keyboard_only;
    int level;
    const char *include, *exclude;
} Options;

typedef struct {
    char *name, *path, *prefix;
    struct stat st;
    bool is_dir, is_link, last;
    int depth;
} Node;

typedef struct { Node *items; size_t len, cap; } Nodes;
typedef struct { char *name, *path; struct stat st; bool is_dir, is_link; } Entry;
typedef struct { Entry *items; size_t len, cap; } Entries;

static volatile sig_atomic_t interrupted, resized;
static int tty_fd = -1;
static struct termios saved_termios;
static bool terminal_active;
static bool mouse_active;

static void *xmalloc(size_t n) {
    void *p = malloc(n ? n : 1);
    if (!p) { perror("malloc"); exit(1); }
    return p;
}
static char *xstrdup(const char *s) {
    char *p = strdup(s);
    if (!p) { perror("strdup"); exit(1); }
    return p;
}
static char *join_path(const char *dir, const char *name) {
    size_t a = strlen(dir), b = strlen(name);
    char *p = xmalloc(a + b + 2);
    memcpy(p, dir, a);
    if (a && dir[a - 1] != '/') p[a++] = '/';
    memcpy(p + a, name, b + 1);
    return p;
}
static void add_node(Nodes *v, Node n) {
    if (v->len == v->cap) {
        v->cap = v->cap ? v->cap * 2 : 64;
        Node *p = realloc(v->items, v->cap * sizeof(*p));
        if (!p) { perror("realloc"); exit(1); }
        v->items = p;
    }
    v->items[v->len++] = n;
}
static void add_entry(Entries *v, Entry e) {
    if (v->len == v->cap) {
        v->cap = v->cap ? v->cap * 2 : 32;
        Entry *p = realloc(v->items, v->cap * sizeof(*p));
        if (!p) { perror("realloc"); exit(1); }
        v->items = p;
    }
    v->items[v->len++] = e;
}
static void free_entries(Entries *v) {
    for (size_t i = 0; i < v->len; i++) {
        free(v->items[i].name);
        free(v->items[i].path);
    }
    free(v->items);
    *v = (Entries){0};
}
static Options sort_options;
static int compare_entries(const void *a0, const void *b0) {
    const Entry *a = a0, *b = b0;
    int c = 0;
    if (sort_options.dirs_first && a->is_dir != b->is_dir)
        return a->is_dir ? -1 : 1;
    if (sort_options.time_sort) {
        if (a->st.st_mtime > b->st.st_mtime) c = -1;
        if (a->st.st_mtime < b->st.st_mtime) c = 1;
    }
    if (!c) c = strcoll(a->name, b->name);
    return sort_options.reverse ? -c : c;
}
static Entries read_entries(const char *path, const Options *o, bool popup) {
    Entries v = {0};
    DIR *dir = opendir(path);
    if (!dir) return v;
    struct dirent *de;
    while ((de = readdir(dir))) {
        const char *name = de->d_name;
        if (!strcmp(name, ".") || !strcmp(name, "..")) continue;
        if (!o->all && name[0] == '.') continue;
        if (o->exclude && fnmatch(o->exclude, name, 0) == 0) continue;
        char *child = join_path(path, name);
        struct stat st;
        if (lstat(child, &st) != 0) { free(child); continue; }
        bool is_dir = S_ISDIR(st.st_mode);
        if (o->dirs_only && !is_dir) { free(child); continue; }
        if (!popup && o->include && !is_dir && fnmatch(o->include, name, 0) != 0) {
            free(child); continue;
        }
        add_entry(&v, (Entry){xstrdup(name), child, st, is_dir, S_ISLNK(st.st_mode)});
    }
    closedir(dir);
    sort_options = *o;
    qsort(v.items, v.len, sizeof(*v.items), compare_entries);
    return v;
}
static char *child_prefix(const Node *parent) {
    size_t n = strlen(parent->prefix);
    char *p = xmalloc(n + 5);
    memcpy(p, parent->prefix, n);
    if (parent->depth > 0) memcpy(p + n, parent->last ? "    " : "│   ", 5);
    else p[n] = '\0';
    return p;
}
static void walk(Nodes *nodes, size_t parent_index, const Options *o) {
    Node *parent = &nodes->items[parent_index];
    if (!parent->is_dir || parent->depth >= o->level) return;
    Entries entries = read_entries(parent->path, o, false);
    for (size_t i = 0; i < entries.len; i++) {
        /* walk can realloc nodes, so do not keep a Node pointer across it. */
        parent = &nodes->items[parent_index];
        Entry *e = &entries.items[i];
        Node n = {xstrdup(e->name), xstrdup(e->path), child_prefix(parent),
                  e->st, e->is_dir, e->is_link, i + 1 == entries.len, parent->depth + 1};
        size_t index = nodes->len;
        add_node(nodes, n);
        walk(nodes, index, o);
    }
    free_entries(&entries);
}
static void free_nodes(Nodes *v) {
    for (size_t i = 0; i < v->len; i++) {
        free(v->items[i].name); free(v->items[i].path); free(v->items[i].prefix);
    }
    free(v->items);
}
static char *absolute_path(const char *path) {
    if (path[0] == '/') return xstrdup(path);
    char *cwd = getcwd(NULL, 0);
    if (!cwd) return xstrdup(path);
    char *out = join_path(cwd, path);
    free(cwd);
    return out;
}
static void appendf(char *buf, size_t cap, size_t *len, const char *fmt, ...) {
    if (*len >= cap) return;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf + *len, cap - *len, fmt, ap);
    va_end(ap);
    if (n > 0) *len += (size_t)n < cap - *len ? (size_t)n : cap - *len - 1;
}
static void append_safe(char *buf, size_t cap, size_t *len, const char *s) {
    if (*len >= cap) return;
    for (const unsigned char *p = (const unsigned char *)s; *p && *len + 1 < cap; p++) {
        buf[(*len)++] = (*p < 32 || *p == 127) ? '?' : (char)*p;
    }
    buf[*len] = '\0';
}
static void mode_string(mode_t mode, char out[11]) {
    out[0] = S_ISDIR(mode) ? 'd' : S_ISLNK(mode) ? 'l' : '-';
    const char *bits = "rwxrwxrwx";
    for (int i = 0; i < 9; i++) out[i + 1] = (mode & (0400 >> i)) ? bits[i] : '-';
    out[10] = '\0';
}
static char *format_node(const Node *n, const Options *o) {
    size_t cap = strlen(n->path) * 2 + strlen(n->prefix) + 128;
    char *buf = xmalloc(cap);
    size_t len = 0;
    buf[0] = '\0';
    if (n->depth && !o->no_indent)
        appendf(buf, cap, &len, "%s%s", n->prefix, n->last ? "└── " : "├── ");
    if (o->permissions) {
        char mode[11]; mode_string(n->st.st_mode, mode);
        appendf(buf, cap, &len, "[%s] ", mode);
    }
    if (o->sizes) {
        if (o->human) {
            double size = (double)n->st.st_size;
            const char *units = "BKMGT"; int u = 0;
            while (size >= 1024 && u < 4) { size /= 1024; u++; }
            appendf(buf, cap, &len, "[%5.1f%c] ", size, units[u]);
        } else appendf(buf, cap, &len, "[%8lld] ", (long long)n->st.st_size);
    }
    append_safe(buf, cap, &len, o->full_path ? n->path : n->name);
    if (o->classify) {
        char suffix = n->is_dir ? '/' : n->is_link ? '@' :
                      (n->st.st_mode & 0111) ? '*' : '\0';
        if (suffix) appendf(buf, cap, &len, "%c", suffix);
    }
    return buf;
}
static void print_tree(const Nodes *nodes, const Options *o, FILE *out) {
    size_t dirs = 0, files = 0;
    for (size_t i = 0; i < nodes->len; i++) {
        char *line = format_node(&nodes->items[i], o);
        fprintf(out, "%s\n", line);
        free(line);
        if (nodes->items[i].depth) {
            if (nodes->items[i].is_dir) dirs++; else files++;
        }
    }
    if (!o->no_report) fprintf(out, "\n%zu director%s, %zu file%s\n", dirs,
                               dirs == 1 ? "y" : "ies", files, files == 1 ? "" : "s");
}
static void terminal_restore(void) {
    if (!terminal_active) return;
    const char *off = mouse_active ?
        "\033[?1000l\033[?1006l\033[?25h\033[?1049l" :
        "\033[?25h\033[?1049l";
    (void)write(tty_fd, off, strlen(off));
    (void)tcsetattr(tty_fd, TCSAFLUSH, &saved_termios);
    terminal_active = false;
}
static void on_signal(int sig) {
    if (sig == SIGWINCH) resized = 1;
    else interrupted = 1;
}
static int terminal_start(bool mouse) {
    const char *term = getenv("TERM");
    if (!term || !strcmp(term, "dumb")) return -1;
    tty_fd = open("/dev/tty", O_RDWR);
    /* Some PTYs and sandboxed terminals have no usable /dev/tty. Bash keeps
       stdin attached to the terminal in process substitutions, while zsh may
       replace it with /dev/null; stderr remains the interactive terminal.
       --emit-cd deliberately reserves stdout for the selected path. */
    if (tty_fd < 0) {
        const int candidates[] = {STDIN_FILENO, STDERR_FILENO};
        for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
            int fd = candidates[i];
            if (!isatty(fd)) continue;
            const char *name = ttyname(fd);
            if (name) tty_fd = open(name, O_RDWR);
            if (tty_fd >= 0) break;
        }
    }
    if (tty_fd < 0 && isatty(STDIN_FILENO)) {
        int flags = fcntl(STDIN_FILENO, F_GETFL);
        if (flags >= 0 && (flags & O_ACCMODE) != O_WRONLY)
            tty_fd = dup(STDIN_FILENO);
    }
    if (tty_fd < 0) return -1;
    if (tcgetattr(tty_fd, &saved_termios) != 0) {
        close(tty_fd);
        tty_fd = -1;
        return -1;
    }
    struct termios raw = saved_termios;
    raw.c_iflag &= (tcflag_t)~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= (tcflag_t)~OPOST;
    raw.c_lflag &= (tcflag_t)~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cflag |= CS8;
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    if (tcsetattr(tty_fd, TCSAFLUSH, &raw) != 0) return -1;
    terminal_active = true;
    mouse_active = mouse;
    atexit(terminal_restore);
    const char *on = mouse ?
        "\033[?1049h\033[?25l\033[?1006h\033[?1000h" :
        "\033[?1049h\033[?25l";
    (void)write(tty_fd, on, strlen(on));
    struct sigaction sa = {0};
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGTSTP, &sa, NULL);
    sigaction(SIGWINCH, &sa, NULL);
    return 0;
}
static void terminal_size(int *rows, int *cols) {
    struct winsize ws = {0};
    if (ioctl(tty_fd, TIOCGWINSZ, &ws) == 0 && ws.ws_row && ws.ws_col) {
        *rows = ws.ws_row; *cols = ws.ws_col;
    } else { *rows = 24; *cols = 80; }
}
static void draw_clipped(FILE *out, const char *s, int width) {
    mbstate_t state = {0};
    int used = 0;
    while (*s && used < width) {
        wchar_t wc;
        size_t n = mbrtowc(&wc, s, MB_CUR_MAX, &state);
        if (n == (size_t)-1 || n == (size_t)-2 || n == 0) {
            fputc('?', out); s++; used++; memset(&state, 0, sizeof(state)); continue;
        }
        int w = wcwidth(wc);
        if (w < 0) w = 1;
        if (used + w > width) break;
        fwrite(s, 1, n, out); s += n; used += w;
    }
}
static long long now_ms(void) {
    struct timeval tv; gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
typedef struct {
    int top, selected, popup_for, popup_row, popup_height, popup_col, popup_width;
    int rows, cols;
    Entries popup;
    int last_click; long long last_click_ms;
} View;
static void close_popup(View *v) { free_entries(&v->popup); v->popup_for = -1; }
static void show_popup(View *v, const Nodes *nodes, const Options *o, int index) {
    close_popup(v);
    v->popup = read_entries(nodes->items[index].path, o, true);
    v->popup_for = index;
}
static void draw_view(FILE *out, View *v, const Nodes *nodes, const Options *o) {
    terminal_size(&v->rows, &v->cols);
    int content = v->rows - 1;
    if (content < 1) content = 1;
    if (v->selected < v->top) v->top = v->selected;
    if (v->selected >= v->top + content) v->top = v->selected - content + 1;
    if (v->top < 0) v->top = 0;
    fputs("\033[H\033[2J", out);
    for (int row = 0; row < content; row++) {
        int i = v->top + row;
        if (i >= (int)nodes->len) break;
        char *line = format_node(&nodes->items[i], o);
        fprintf(out, "\033[%d;1H", row + 1);
        if (i == v->selected) fputs("\033[7m", out);
        draw_clipped(out, line, v->cols);
        if (i == v->selected) fputs("\033[0m", out);
        free(line);
    }
    v->popup_height = 0;
    if (v->popup_for >= 0 && v->cols >= 18 && content >= 5) {
        int anchor = v->popup_for - v->top + 1;
        int available = content - 2;
        int shown = (int)v->popup.len;
        if (shown == 0) shown = 1;
        if (shown > 9) shown = 9;
        if (shown > available) shown = available;
        int height = shown + 2;
        int row = anchor + 1;
        if (row + height - 1 > content) row = anchor - height;
        if (row < 1) row = 1;
        int width = v->cols < 62 ? v->cols - 2 : 60;
        int col = v->cols - width + 1;
        v->popup_row = row; v->popup_col = col;
        v->popup_height = height; v->popup_width = width;
        for (int y = 0; y < height; y++) {
            fprintf(out, "\033[%d;%dH\033[0m", row + y, col);
            fputc(y == 0 || y == height - 1 ? '+' : '|', out);
            if (y == 0 || y == height - 1) {
                for (int x = 0; x < width - 2; x++) fputc('-', out);
            } else {
                char *line;
                if (v->popup.len == 0) line = xstrdup("(empty)");
                else if (y == height - 2 && (int)v->popup.len > shown) {
                    line = xstrdup("...");
                } else if (y - 1 < (int)v->popup.len) {
                    Entry *e = &v->popup.items[y - 1];
                    size_t cap = strlen(e->name) + 4;
                    line = xmalloc(cap);
                    snprintf(line, cap, "%s%s", e->name, e->is_dir ? "/" : "");
                } else line = xstrdup("...");
                /* Fill the interior first; cursor positioning prevents stale text. */
                for (int x = 0; x < width - 2; x++) fputc(' ', out);
                fprintf(out, "\033[%d;%dH", row + y, col + 1);
                size_t safe_cap = strlen(line) + 1, safe_len = 0;
                char *safe = xmalloc(safe_cap);
                safe[0] = '\0';
                append_safe(safe, safe_cap, &safe_len, line);
                draw_clipped(out, safe, width - 2);
                free(safe);
                free(line);
            }
            fprintf(out, "\033[%d;%dH", row + y, col + width - 1);
            fputc(y == 0 || y == height - 1 ? '+' : '|', out);
        }
    }
    fprintf(out, "\033[%d;1H\033[0m", v->rows);
    draw_clipped(out, "j/k move  Space/Right preview  Enter cd  Esc/Left close  q quit", v->cols);
    fflush(out);
}
static bool in_popup(const View *v, int x, int y) {
    return v->popup_height && x >= v->popup_col && x < v->popup_col + v->popup_width &&
           y >= v->popup_row && y < v->popup_row + v->popup_height;
}
static int read_event(int fd, int *x, int *y, bool *click) {
    unsigned char ch;
    ssize_t n = read(fd, &ch, 1);
    if (n <= 0) return 0;
    *click = false;
    if (ch != 27) return ch;
    unsigned char seq[64]; size_t len = 0;
    while (len < sizeof(seq) - 1) {
        n = read(fd, &seq[len], 1);
        if (n <= 0) break;
        if ((seq[len] >= '@' && seq[len] <= '~') && len >= 1) { len++; break; }
        len++;
    }
    seq[len] = '\0';
    if (len >= 2 && seq[0] == '[' && seq[1] == '<') {
        int button, mx, my; char kind;
        if (sscanf((char *)seq + 2, "%d;%d;%d%c", &button, &mx, &my, &kind) == 4) {
            if (button == 0 && kind == 'm') {
                *x = mx; *y = my; *click = true; return 0;
            }
            if (kind == 'M' && button == 64) return 'K';
            if (kind == 'M' && button == 65) return 'J';
        }
    }
    if (len == 2 && seq[0] == '[' && seq[1] == 'A') return 'k';
    if (len == 2 && seq[0] == '[' && seq[1] == 'B') return 'j';
    if (len == 2 && seq[0] == '[' && seq[1] == 'C') return 'l';
    if (len == 2 && seq[0] == '[' && seq[1] == 'D') return 'h';
    if (len == 3 && seq[0] == '[' && seq[1] == '5' && seq[2] == '~') return 'K';
    if (len == 3 && seq[0] == '[' && seq[1] == '6' && seq[2] == '~') return 'J';
    return 27;
}
static int interactive(const Nodes *nodes, const Options *o) {
    if (terminal_start(!o->keyboard_only) != 0) return -1;
    FILE *out = fdopen(dup(tty_fd), "w");
    if (!out) { terminal_restore(); return -1; }
    View v = {.popup_for = -1, .last_click = -1};
    int chosen = -1;
    bool redraw = true;
    while (!interrupted) {
        if (redraw || resized) { resized = 0; draw_view(out, &v, nodes, o); redraw = false; }
        int x = 0, y = 0; bool click = false;
        int key = read_event(tty_fd, &x, &y, &click);
        if (click && !o->keyboard_only) {
            if (in_popup(&v, x, y)) continue;
            int i = v.top + y - 1;
            if (y < 1 || y >= v.rows || i >= (int)nodes->len) continue;
            v.selected = i;
            if (nodes->items[i].is_dir) {
                long long now = now_ms();
                if (v.last_click == i && now - v.last_click_ms <= 450) { chosen = i; break; }
                show_popup(&v, nodes, o, i);
                v.last_click = i; v.last_click_ms = now;
            } else { close_popup(&v); v.last_click = -1; }
            redraw = true;
        } else if (key == 'q' || key == 3 || key == 26) break;
        else if (key == 'j' || key == 'k' || key == 'J' || key == 'K') {
            int step = (key == 'J' || key == 'K') ? 5 : 1;
            int next = v.selected + ((key == 'j' || key == 'J') ? step : -step);
            if (next < 0) next = 0;
            if (next >= (int)nodes->len) next = (int)nodes->len - 1;
            if (next >= 0 && next < (int)nodes->len) v.selected = next;
            close_popup(&v); v.last_click = -1; redraw = true;
        } else if (key == ' ' || key == 'l') {
            if (nodes->items[v.selected].is_dir) {
                if (v.popup_for == v.selected) close_popup(&v);
                else show_popup(&v, nodes, o, v.selected);
                redraw = true;
            }
        } else if (key == '\r' || key == '\n') {
            if (nodes->items[v.selected].is_dir) { chosen = v.selected; break; }
        } else if (key == 27 || key == 'h') { close_popup(&v); redraw = true; }
    }
    close_popup(&v);
    fclose(out);
    terminal_restore();
    if (chosen >= 0) {
        char *path = absolute_path(nodes->items[chosen].path);
        if (o->emit_cd) fwrite(path, 1, strlen(path) + 1, stdout);
        else printf("%s\n", path);
        free(path);
    }
    return interrupted ? 130 : 0;
}
static void usage(FILE *out) {
    fputs("Usage: ctree [options] [directory ...]\n"
          "  -a             include hidden entries\n"
          "  -d             directories only\n"
          "  -L N           limit depth (1 shows immediate children)\n"
          "  -I PATTERN     exclude matching names (shell glob)\n"
          "  -P PATTERN     show matching files; keep directories for navigation\n"
          "  -f -p -s -h    full paths, permissions, sizes, human sizes\n"
          "  -t -r          sort by mtime, reverse sort\n"
          "  -i -F          omit branches, append type marks\n"
          "  --dirsfirst    list directories first\n"
          "  --noreport     omit totals\n"
          "  --plain        print a static tree, even on a terminal\n"
          "  --keyboard-only  do not enable terminal mouse reporting\n"
          "  --emit-cd      emit selected path as NUL for a shell function\n"
          "  --help         show this help\n", out);
}
static int parse(int argc, char **argv, Options *o, int *first_path) {
    *o = (Options){.level = 1000000};
    int i = 1;
    for (; i < argc; i++) {
        const char *arg = argv[i];
        if (!strcmp(arg, "--")) { i++; break; }
        if (arg[0] != '-' || !arg[1]) break;
        if (!strcmp(arg, "--help")) { usage(stdout); exit(0); }
        if (!strcmp(arg, "--version")) { puts("ctree 0.1.0"); exit(0); }
        if (!strcmp(arg, "--dirsfirst")) { o->dirs_first = true; continue; }
        if (!strcmp(arg, "--noreport")) { o->no_report = true; continue; }
        if (!strcmp(arg, "--plain")) { o->plain = true; continue; }
        if (!strcmp(arg, "--keyboard-only")) { o->keyboard_only = true; continue; }
        if (!strcmp(arg, "--emit-cd")) { o->emit_cd = true; continue; }
        for (const char *p = arg + 1; *p; p++) {
            switch (*p) {
                case 'a': o->all = true; break;
                case 'd': o->dirs_only = true; break;
                case 'f': o->full_path = true; break;
                case 'p': o->permissions = true; break;
                case 's': o->sizes = true; break;
                case 'h': o->human = true; o->sizes = true; break;
                case 't': o->time_sort = true; break;
                case 'r': o->reverse = true; break;
                case 'i': o->no_indent = true; break;
                case 'F': o->classify = true; break;
                case 'n': break; /* Color is off by default. */
                case 'L': case 'I': case 'P': {
                    const char *value = p[1] ? p + 1 : (i + 1 < argc ? argv[++i] : NULL);
                    if (!value) { fprintf(stderr, "ctree: -%c needs a value\n", *p); return -1; }
                    if (*p == 'L') {
                        char *end; long depth = strtol(value, &end, 10);
                        if (!*value || *end || depth < 1 || depth > 1000000) {
                            fprintf(stderr, "ctree: invalid depth: %s\n", value); return -1;
                        }
                        o->level = (int)depth;
                    } else if (*p == 'I') o->exclude = value;
                    else o->include = value;
                    p += strlen(p + 1); break;
                }
                default: fprintf(stderr, "ctree: unsupported option: -%c\n", *p); return -1;
            }
        }
    }
    *first_path = i;
    return 0;
}
int main(int argc, char **argv) {
    setlocale(LC_ALL, "");
    Options o; int first;
    if (parse(argc, argv, &o, &first) != 0) return 2;
    Nodes nodes = {0};
    int count = argc - first;
    for (int j = 0; j < (count ? count : 1); j++) {
        const char *path = count ? argv[first + j] : ".";
        struct stat st;
        if (lstat(path, &st) != 0) { fprintf(stderr, "ctree: %s: %s\n", path, strerror(errno)); continue; }
        size_t root = nodes.len;
        add_node(&nodes, (Node){xstrdup(path), xstrdup(path), xstrdup(""), st,
                                S_ISDIR(st.st_mode), S_ISLNK(st.st_mode), true, 0});
        walk(&nodes, root, &o);
    }
    if (!nodes.len) { free_nodes(&nodes); return 1; }
    int rc = 0;
    if (!o.plain && (o.emit_cd || (isatty(STDIN_FILENO) && isatty(STDOUT_FILENO)))) {
        rc = interactive(&nodes, &o);
        if (rc == -1) {
            fprintf(stderr, "ctree: no interactive terminal; printing tree\n");
            print_tree(&nodes, &o, o.emit_cd ? stderr : stdout);
            rc = 0;
        }
    } else print_tree(&nodes, &o, stdout);
    free_nodes(&nodes);
    return rc;
}
