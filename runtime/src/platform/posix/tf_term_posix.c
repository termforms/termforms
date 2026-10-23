/* SPDX-License-Identifier: 0BSD */
/*
 * tf_term_posix.c -- terminal layer for Linux, macOS and other POSIX systems.
 *
 * Raw mode via termios, the alternate screen via VT sequences, and signal
 * handlers that put the terminal back before the process dies, so a crash or
 * a dropped SSH session never leaves the user's shell unusable.
 */
#if !defined(_WIN32)

/* Feature macros must precede every system header. SIGWINCH and TIOCGWINSZ
 * are not POSIX, so the BSD/Darwin extensions are requested as well. */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#define _DARWIN_C_SOURCE

#include "../../tui/tf_tui.h"

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

/* Leave the alternate screen last, so the user's scrollback reappears. */
static const char ENTER_SEQ[] = "\x1b[?1049h" /* alternate screen */
                                "\x1b[?7l"    /* no auto-wrap at the last column */
                                "\x1b[H\x1b[2J";
static const char LEAVE_SEQ[] = "\x1b[0m\x1b[?25h\x1b[?7h\x1b[?1049l";

static struct termios saved_termios;
static volatile sig_atomic_t active = 0;
static volatile sig_atomic_t resized = 0;

/* Async-signal-safe: only write() and tcsetattr(). */
static void restore_raw(void)
{
    if (active)
    {
        active = 0;
        ssize_t ignored = write(STDOUT_FILENO, LEAVE_SEQ, sizeof LEAVE_SEQ - 1);
        (void)ignored;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_termios);
    }
}

static void on_winch(int sig)
{
    (void)sig;
    resized = 1;
}

/* Fatal signals: restore the terminal, then die of the same signal. */
static void on_fatal(int sig)
{
    restore_raw();
    signal(sig, SIG_DFL);
    raise(sig);
}

static void install(int sig, void (*handler)(int))
{
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    /* No SA_RESTART: SIGWINCH must interrupt select() so a resize is seen. */
    sigaction(sig, &sa, NULL);
}

int tf_term_open(const char **reason)
{
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO))
    {
        *reason = "standard input and output must be a terminal";
        return -1;
    }
    if (tcgetattr(STDIN_FILENO, &saved_termios) != 0)
    {
        *reason = "cannot read the terminal settings";
        return -1;
    }

    struct termios raw = saved_termios;
    raw.c_iflag &= (tcflag_t) ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= (tcflag_t) ~(OPOST);
    raw.c_cflag |= (tcflag_t)CS8;
    /* No ISIG: Ctrl+C, Ctrl+Z and Ctrl+\ arrive as keys, not signals. */
    raw.c_lflag &= (tcflag_t) ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0)
    {
        *reason = "cannot switch the terminal to raw mode";
        return -1;
    }
    active = 1;

    install(SIGWINCH, on_winch);
    install(SIGHUP, on_fatal); /* SSH connection dropped */
    install(SIGTERM, on_fatal);
    install(SIGINT, on_fatal);
    install(SIGQUIT, on_fatal);
    install(SIGSEGV, on_fatal);
    install(SIGBUS, on_fatal);
    install(SIGFPE, on_fatal);
    install(SIGABRT, on_fatal);

    tf_term_write(ENTER_SEQ, sizeof ENTER_SEQ - 1);
    return 0;
}

void tf_term_close(void)
{
    restore_raw();
}

void tf_term_write(const char *bytes, size_t length)
{
    while (length > 0)
    {
        const ssize_t n = write(STDOUT_FILENO, bytes, length);
        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return; /* terminal gone (e.g. SSH dropped); SIGHUP will follow */
        }
        bytes += n;
        length -= (size_t)n;
    }
}

int tf_term_read(unsigned char *buffer, int capacity, int timeout_ms)
{
    fd_set readable;
    FD_ZERO(&readable);
    FD_SET(STDIN_FILENO, &readable);

    struct timeval tv;
    struct timeval *wait = NULL;
    if (timeout_ms >= 0)
    {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        wait = &tv;
    }

    const int ready = select(STDIN_FILENO + 1, &readable, NULL, NULL, wait);
    if (ready <= 0)
    {
        return 0; /* timeout, or EINTR from SIGWINCH */
    }

    const ssize_t n = read(STDIN_FILENO, buffer, (size_t)capacity);
    if (n == 0)
    {
        /* End of input: the terminal has gone away. */
        tf_runtime_error(TF_ERR_TERMINAL, "Terminal closed");
    }
    return n > 0 ? (int)n : 0;
}

void tf_term_size(int *cols, int *rows)
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0)
    {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
    }
    else
    {
        *cols = 80;
        *rows = 24;
    }
}

int tf_term_take_resize(void)
{
    if (resized)
    {
        resized = 0;
        return 1;
    }
    return 0;
}

int tf_term_utf8(void)
{
    /* The locale variables decide, in the order the C library consults them. */
    const char *names[] = {"LC_ALL", "LC_CTYPE", "LANG"};
    for (size_t i = 0; i < sizeof names / sizeof names[0]; ++i)
    {
        const char *value = getenv(names[i]);
        if (value != NULL && value[0] != '\0')
        {
            return strstr(value, "UTF-8") != NULL || strstr(value, "utf8") != NULL ||
                   strstr(value, "UTF8") != NULL || strstr(value, "utf-8") != NULL;
        }
    }
    return 0;
}

const char *tf_platform_getenv(const char *name)
{
    return getenv(name);
}

#else
/* ISO C forbids an empty translation unit. */
typedef int tf_term_posix_not_used;
#endif
