/* SPDX-License-Identifier: 0BSD */
/*
 * tf_forms.c -- forms, controls, rendering and the event loop.
 *
 * Layout model (as in VB-DOS): a form has a fixed size in character cells
 * and is centred on a plain backdrop. Width and Height include the border.
 * Control positions (Left, Top) are relative to the form's client area, the
 * space inside the border.
 *
 * This version supports TextBox and CommandButton. Tab and Shift+Tab move
 * the focus, Enter or Space clicks a button, Ctrl+Q closes the form.
 */
#include "tf_tui.h"

#include <stdlib.h>
#include <string.h>

struct tf_control
{
    tf_control_type type;
    char *name;
    tf_control *form; /* owning form; the form itself for a form */
    tf_string *caption;
    tf_string *text;
    int32_t left;
    int32_t top;
    int32_t width;
    int32_t height;
    tf_handler handlers[TF_EVENT_COUNT];

    /* Form only: children in tab order, and the focused child's index. */
    tf_control **children;
    int child_count;
    int child_capacity;
    int focus;

    /* TextBox only: cursor as a byte offset into text, and the index of the
     * first visible code point. */
    int32_t cursor;
    int32_t scroll;
};

/* ------------------------------------------------------------------------ */
/* Creation and properties                                                  */
/* ------------------------------------------------------------------------ */

static char *copy_name(const char *name)
{
    const size_t length = strlen(name);
    char *copy = tf_xmalloc(length + 1);
    memcpy(copy, name, length + 1);
    return copy;
}

tf_control *tf_form_new(const char *name)
{
    tf_control *form = tf_xcalloc(1, sizeof *form);
    form->type = TF_CTL_FORM;
    form->name = copy_name(name);
    form->form = form;
    form->width = 40;
    form->height = 10;
    form->focus = -1;
    return form;
}

tf_control *tf_control_new(tf_control *form, tf_control_type type, const char *name)
{
    tf_control *control = tf_xcalloc(1, sizeof *control);
    control->type = type;
    control->name = copy_name(name);
    control->form = form;
    control->width = type == TF_CTL_TEXTBOX ? 20 : 10;
    control->height = 1;

    if (form->child_count == form->child_capacity)
    {
        form->child_capacity = form->child_capacity == 0 ? 8 : form->child_capacity * 2;
        form->children =
            tf_xrealloc(form->children, (size_t)form->child_capacity * sizeof *form->children);
    }
    form->children[form->child_count++] = control;
    return control;
}

void tf_control_on(tf_control *control, tf_event event, tf_handler handler)
{
    control->handlers[event] = handler;
}

static void fire(tf_control *control, tf_event event)
{
    if (control->handlers[event] != NULL)
    {
        control->handlers[event]();
    }
}

void tf_set_str(tf_control *control, tf_prop prop, const tf_string *value)
{
    /* Retain before releasing, in case value is the current string. */
    tf_string *v = (tf_string *)value;
    tf_str_retain(v);
    if (prop == TF_PROP_TEXT)
    {
        tf_str_assign(&control->text, v);
        control->cursor = tf_str_lenb(v); /* cursor to the end, as in VB */
        control->scroll = 0;
    }
    else
    {
        tf_str_assign(&control->caption, v);
    }
}

tf_string *tf_get_str(const tf_control *control, tf_prop prop)
{
    return prop == TF_PROP_TEXT ? control->text : control->caption;
}

void tf_set_int(tf_control *control, tf_prop prop, int32_t value)
{
    /* Sizes below 1 would make a control vanish or break layout. */
    switch (prop)
    {
        case TF_PROP_LEFT:
            control->left = value;
            break;
        case TF_PROP_TOP:
            control->top = value;
            break;
        case TF_PROP_WIDTH:
            control->width = value < 1 ? 1 : value;
            break;
        case TF_PROP_HEIGHT:
            control->height = value < 1 ? 1 : value;
            break;
        default:
            break;
    }
}

int32_t tf_get_int(const tf_control *control, tf_prop prop)
{
    switch (prop)
    {
        case TF_PROP_LEFT:
            return control->left;
        case TF_PROP_TOP:
            return control->top;
        case TF_PROP_WIDTH:
            return control->width;
        case TF_PROP_HEIGHT:
            return control->height;
        default:
            return 0;
    }
}

/* ------------------------------------------------------------------------ */
/* Rendering                                                                */
/* ------------------------------------------------------------------------ */

static int use_utf8 = 1;

static uint32_t box(uint32_t utf8_char, char ascii)
{
    return use_utf8 ? utf8_char : (uint32_t)(unsigned char)ascii;
}

/* Copies a caption without '&' accelerator markers ("&&" is a literal &). */
static void strip_accelerator(const char *caption, char *out, size_t capacity)
{
    size_t used = 0;
    for (const char *p = caption; *p != '\0' && used + 1 < capacity; ++p)
    {
        if (*p == '&')
        {
            if (p[1] != '&')
            {
                continue;
            }
            ++p;
        }
        out[used++] = *p;
    }
    out[used] = '\0';
}

/* Number of code points in the first byte_offset bytes of s. */
static int32_t code_points_before(const tf_string *s, int32_t byte_offset)
{
    const char *bytes = tf_str_cstr(s);
    int32_t count = 0;
    for (int32_t i = 0; i < byte_offset; ++i)
    {
        if (((unsigned char)bytes[i] & 0xC0u) != 0x80u)
        {
            count++;
        }
    }
    return count;
}

/* Cells needed to show UTF-8 text (one per code point in this version). */
static int utf8_cells(const char *text)
{
    int cells = 0;
    for (const char *p = text; *p != '\0'; ++p)
    {
        if (((unsigned char)*p & 0xC0u) != 0x80u)
        {
            cells++;
        }
    }
    return cells;
}

/* Byte offset of the code point with the given index. */
static const char *skip_code_points(const char *bytes, int32_t count)
{
    while (count > 0 && *bytes != '\0')
    {
        uint32_t cp = 0;
        bytes += tf_utf8_decode(bytes, (int)strlen(bytes), &cp);
        count--;
    }
    return bytes;
}

static void draw_textbox(tf_control *tb, int x, int y, int max_width, int focused)
{
    const int w = tb->width < max_width ? (int)tb->width : max_width;
    if (w <= 0)
    {
        return;
    }
    tf_screen_fill(x, y, w, 1, ' ', TF_BLACK, TF_WHITE);

    /* Keep the cursor inside the visible window. */
    const int32_t cursor_cp = code_points_before(tb->text, tb->cursor);
    if (cursor_cp < tb->scroll)
    {
        tb->scroll = cursor_cp;
    }
    else if (cursor_cp >= tb->scroll + w)
    {
        tb->scroll = cursor_cp - w + 1;
    }

    tf_screen_text(x, y, skip_code_points(tf_str_cstr(tb->text), tb->scroll), w, TF_BLACK,
                   TF_WHITE);
    if (focused)
    {
        tf_screen_cursor(x + (int)(cursor_cp - tb->scroll), y, 1);
    }
}

static void draw_button(const tf_control *button, int x, int y, int max_width, int focused)
{
    const int w = button->width < max_width ? (int)button->width : max_width;
    if (w <= 0)
    {
        return;
    }
    const int fg = TF_BLACK;
    const int bg = focused ? TF_CYAN : TF_GREY;

    char caption[256];
    strip_accelerator(tf_str_cstr(button->caption), caption, sizeof caption);
    char label[300];
    snprintf(label, sizeof label, "< %s >", caption);

    /* Centre the label; a label wider than the button is cut on the right. */
    const int cells = utf8_cells(label);
    const int pad = cells < w ? (w - cells) / 2 : 0;
    tf_screen_fill(x, y, w, 1, ' ', fg, bg);
    tf_screen_text(x + pad, y, label, w - pad, fg, bg);
}

static void draw_form(tf_control *form)
{
    int cols = 0;
    int rows = 0;
    tf_screen_size(&cols, &rows);
    tf_screen_cursor(0, 0, 0);

    const int fw = (int)form->width;
    const int fh = (int)form->height;

    if (cols < fw || rows < fh)
    {
        char message[96];
        snprintf(message, sizeof message, "Please resize the terminal to at least %dx%d", fw, fh);
        tf_screen_fill(0, 0, cols, rows, ' ', TF_GREY, TF_BLACK);
        const int length = (int)strlen(message);
        const int x = length < cols ? (cols - length) / 2 : 0;
        tf_screen_text(x, rows / 2, message, cols, TF_WHITE, TF_BLACK);
        return;
    }

    /* Backdrop, then the form centred on it. */
    tf_screen_fill(0, 0, cols, rows, ' ', TF_GREY, TF_BLUE);
    const int ox = (cols - fw) / 2;
    const int oy = (rows - fh) / 2;
    tf_screen_fill(ox, oy, fw, fh, ' ', TF_BLACK, TF_GREY);

    /* Border. */
    for (int x = ox + 1; x < ox + fw - 1; ++x)
    {
        tf_screen_put(x, oy, box(0x2500, '-'), TF_BLACK, TF_GREY);
        tf_screen_put(x, oy + fh - 1, box(0x2500, '-'), TF_BLACK, TF_GREY);
    }
    for (int y = oy + 1; y < oy + fh - 1; ++y)
    {
        tf_screen_put(ox, y, box(0x2502, '|'), TF_BLACK, TF_GREY);
        tf_screen_put(ox + fw - 1, y, box(0x2502, '|'), TF_BLACK, TF_GREY);
    }
    tf_screen_put(ox, oy, box(0x250C, '+'), TF_BLACK, TF_GREY);
    tf_screen_put(ox + fw - 1, oy, box(0x2510, '+'), TF_BLACK, TF_GREY);
    tf_screen_put(ox, oy + fh - 1, box(0x2514, '+'), TF_BLACK, TF_GREY);
    tf_screen_put(ox + fw - 1, oy + fh - 1, box(0x2518, '+'), TF_BLACK, TF_GREY);

    /* Title, centred in the top border. */
    if (tf_str_lenb(form->caption) > 0)
    {
        char title[256];
        snprintf(title, sizeof title, " %s ", tf_str_cstr(form->caption));
        const int cells = utf8_cells(title);
        if (cells <= fw - 2)
        {
            tf_screen_text(ox + (fw - cells) / 2, oy, title, cells, TF_BLACK, TF_GREY);
        }
    }

    /* Controls, clipped to the client area. */
    const int client_x = ox + 1;
    const int client_y = oy + 1;
    const int client_w = fw - 2;
    const int client_h = fh - 2;
    for (int i = 0; i < form->child_count; ++i)
    {
        tf_control *c = form->children[i];
        if (c->left < 0 || c->top < 0 || c->left >= client_w || c->top >= client_h)
        {
            continue;
        }
        const int x = client_x + (int)c->left;
        const int y = client_y + (int)c->top;
        const int room = client_w - (int)c->left;
        const int focused = i == form->focus;
        if (c->type == TF_CTL_TEXTBOX)
        {
            draw_textbox(c, x, y, room, focused);
        }
        else if (c->type == TF_CTL_COMMANDBUTTON)
        {
            draw_button(c, x, y, room, focused);
        }
    }
}

/* ------------------------------------------------------------------------ */
/* Keyboard handling                                                        */
/* ------------------------------------------------------------------------ */

static int focusable(const tf_control *c)
{
    return c->type == TF_CTL_TEXTBOX || c->type == TF_CTL_COMMANDBUTTON;
}

static void move_focus(tf_control *form, int step)
{
    if (form->child_count == 0)
    {
        return;
    }
    int index = form->focus;
    for (int tries = 0; tries < form->child_count; ++tries)
    {
        index = (index + step + form->child_count) % form->child_count;
        if (focusable(form->children[index]))
        {
            form->focus = index;
            return;
        }
    }
}

/* Replaces bytes [from, to) of the text with insert, keeping the cursor
 * after the inserted text. */
static void edit_text(tf_control *tb, int32_t from, int32_t to, const char *insert, int32_t length)
{
    const char *old = tf_str_cstr(tb->text);
    const int32_t old_length = tf_str_lenb(tb->text);
    const int32_t new_length = old_length - (to - from) + length;

    char *buffer = tf_xmalloc((size_t)new_length + 1);
    memcpy(buffer, old, (size_t)from);
    memcpy(buffer + from, insert, (size_t)length);
    memcpy(buffer + from + length, old + to, (size_t)(old_length - to));

    tf_str_assign(&tb->text, tf_str_from_utf8(buffer, new_length));
    free(buffer);
    tb->cursor = from + length;
    fire(tb, TF_EVENT_CHANGE);
}

static int32_t previous_boundary(const tf_control *tb)
{
    const char *bytes = tf_str_cstr(tb->text);
    int32_t i = tb->cursor;
    while (i > 0)
    {
        --i;
        if (((unsigned char)bytes[i] & 0xC0u) != 0x80u)
        {
            break;
        }
    }
    return i;
}

static int32_t next_boundary(const tf_control *tb)
{
    const char *bytes = tf_str_cstr(tb->text);
    const int32_t length = tf_str_lenb(tb->text);
    int32_t i = tb->cursor;
    if (i < length)
    {
        ++i;
        while (i < length && ((unsigned char)bytes[i] & 0xC0u) == 0x80u)
        {
            ++i;
        }
    }
    return i;
}

static void textbox_key(tf_control *tb, tf_key key)
{
    switch (key.kind)
    {
        case TF_KEY_CHAR:
        {
            char encoded[4];
            const int n = tf_utf8_encode(key.ch, encoded);
            edit_text(tb, tb->cursor, tb->cursor, encoded, n);
            break;
        }
        case TF_KEY_BACKSPACE:
            if (tb->cursor > 0)
            {
                edit_text(tb, previous_boundary(tb), tb->cursor, "", 0);
            }
            break;
        case TF_KEY_DELETE:
            if (tb->cursor < tf_str_lenb(tb->text))
            {
                const int32_t at = tb->cursor;
                edit_text(tb, at, next_boundary(tb), "", 0);
                tb->cursor = at;
            }
            break;
        case TF_KEY_LEFT:
            tb->cursor = previous_boundary(tb);
            break;
        case TF_KEY_RIGHT:
            tb->cursor = next_boundary(tb);
            break;
        case TF_KEY_HOME:
            tb->cursor = 0;
            break;
        case TF_KEY_END:
            tb->cursor = tf_str_lenb(tb->text);
            break;
        default:
            break;
    }
}

static void button_key(tf_control *button, tf_key key)
{
    if (key.kind == TF_KEY_ENTER || (key.kind == TF_KEY_CHAR && key.ch == ' '))
    {
        fire(button, TF_EVENT_CLICK);
    }
}

/* ------------------------------------------------------------------------ */
/* Showing the form                                                         */
/* ------------------------------------------------------------------------ */

static int headless = 0;

static void parse_size(const char *text, int *cols, int *rows)
{
    int c = 0;
    int r = 0;
    if (text != NULL && sscanf(text, "%dx%d", &c, &r) == 2 && c > 0 && r > 0 && c <= 1000 &&
        r <= 1000)
    {
        *cols = c;
        *rows = r;
    }
}

/* Registered as the shutdown hook: runs on normal close, End, or an error. */
static void ui_shutdown(void)
{
    if (headless)
    {
        tf_screen_dump(stdout);
    }
    else
    {
        tf_term_close();
    }
    tf_screen_free();
}

static void ui_open(void)
{
    /* Copied: tf_platform_getenv's result only lives until its next call. */
    static char *script = NULL;
    const char *headless_keys = tf_platform_getenv("TERMFORMS_HEADLESS");
    int cols = 80;
    int rows = 25;

    if (headless_keys != NULL)
    {
        const size_t length = strlen(headless_keys);
        script = tf_xmalloc(length + 1);
        memcpy(script, headless_keys, length + 1);
        headless = 1;
        use_utf8 = 1; /* deterministic output for tests */
        parse_size(tf_platform_getenv("TERMFORMS_HEADLESS_SIZE"), &cols, &rows);
        tf_screen_init(cols, rows, 0);
        tf_input_start(script);
    }
    else
    {
        const char *reason = "";
        if (tf_term_open(&reason) != 0)
        {
            tf_runtime_error(TF_ERR_TERMINAL, reason);
        }
        use_utf8 = tf_term_utf8();
        tf_term_size(&cols, &rows);
        tf_screen_init(cols, rows, 1);
        tf_input_start(NULL);
    }
    tf_set_shutdown_hook(ui_shutdown);
}

int tf_form_run(tf_control *form)
{
    ui_open();
    move_focus(form, 1);

    for (;;)
    {
        draw_form(form);
        tf_screen_flush();

        const tf_key key = tf_input_next();
        if (key.kind == TF_KEY_EOF || (key.kind == TF_KEY_CTRL && key.ch == 'q'))
        {
            break;
        }
        if (key.kind == TF_KEY_RESIZE)
        {
            if (!headless)
            {
                int cols = 80;
                int rows = 24;
                tf_term_size(&cols, &rows);
                tf_screen_init(cols, rows, 1);
            }
            continue;
        }
        if (key.kind == TF_KEY_TAB)
        {
            move_focus(form, 1);
            continue;
        }
        if (key.kind == TF_KEY_BACKTAB)
        {
            move_focus(form, -1);
            continue;
        }
        if (form->focus >= 0)
        {
            tf_control *focused = form->children[form->focus];
            if (focused->type == TF_CTL_TEXTBOX)
            {
                textbox_key(focused, key);
            }
            else
            {
                button_key(focused, key);
            }
        }
    }

    /* Draw the final state so a headless dump shows what the user last saw. */
    draw_form(form);
    tf_run_shutdown_hook();
    return 0;
}
