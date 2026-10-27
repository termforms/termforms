/* SPDX-License-Identifier: 0BSD */
/*
 * tf_forms.c -- forms, controls, rendering and the event loop.
 *
 * Layout model (as in VB-DOS): a form has a fixed size in character cells
 * and is centred on a plain backdrop. Width and Height include the border.
 * Control positions (Left, Top) are relative to the form's client area, the
 * space inside the border.
 *
 * Controls: Label, TextBox, CommandButton, CheckBox, OptionButton and Frame.
 * A Frame contains controls: their Left and Top are inside it, and the
 * OptionButtons in one container (the form or a Frame) are a group.
 *
 * Keys (as VB): Tab and Shift+Tab move between controls, stopping once in
 * an OptionButton group (on its selected button); arrows move within the
 * group, selecting as they go. Space presses a button, toggles a CheckBox
 * and selects an OptionButton. Enter presses the focused button, or else
 * the Default one; Esc presses the Cancel one. Alt+letter is the access
 * key of a Caption with &letter. Ctrl+F4 (Alt+F4) closes the form.
 */
#include "tf_tui.h"

#include <stdlib.h>
#include <string.h>

struct tf_control
{
    tf_control_type type;
    char *name;
    tf_control *form;   /* owning form; the form itself for a form */
    tf_control *parent; /* the form, or the Frame it is in */
    tf_string *caption;
    tf_string *text;
    int32_t left;
    int32_t top;
    int32_t width;
    int32_t height;
    int32_t value; /* CheckBox 0/1/2, OptionButton 0/1 */
    int is_default;
    int is_cancel;
    int enabled;
    int visible;
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
    form->parent = form;
    form->width = 40;
    form->height = 10;
    form->focus = -1;
    form->enabled = 1;
    form->visible = 1;
    return form;
}

tf_control *tf_control_new(tf_control *form, tf_control_type type, const char *name)
{
    tf_control *control = tf_xcalloc(1, sizeof *control);
    control->type = type;
    control->name = copy_name(name);
    control->form = form;
    control->parent = form;
    control->width = type == TF_CTL_TEXTBOX ? 20 : type == TF_CTL_FRAME ? 20 : 10;
    control->height = type == TF_CTL_FRAME ? 5 : 1;
    control->enabled = 1;
    control->visible = 1;

    if (form->child_count == form->child_capacity)
    {
        form->child_capacity = form->child_capacity == 0 ? 8 : form->child_capacity * 2;
        form->children =
            tf_xrealloc(form->children, (size_t)form->child_capacity * sizeof *form->children);
    }
    form->children[form->child_count++] = control;
    return control;
}

void tf_control_container(tf_control *control, tf_control *frame)
{
    control->parent = frame;
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
        /* An error a Try's Finally let through, with no Catch left. */
        tf_check_uncaught();
    }
}

/* Selects an OptionButton: the others in its group are cleared. Click
   fires when it was not already selected (by the user or by code, as VB). */
static void select_option(tf_control *option)
{
    const tf_control *form = option->form;
    const int was = option->value;
    for (int i = 0; i < form->child_count; ++i)
    {
        tf_control *c = form->children[i];
        if (c->type == TF_CTL_OPTIONBUTTON && c->parent == option->parent)
        {
            c->value = 0;
        }
    }
    option->value = 1;
    if (!was)
    {
        fire(option, TF_EVENT_CLICK);
    }
}

/* Only one Default (and one Cancel) button per form, as VB. */
static void make_only(tf_control *button, tf_prop prop)
{
    const tf_control *form = button->form;
    for (int i = 0; i < form->child_count; ++i)
    {
        tf_control *c = form->children[i];
        if (prop == TF_PROP_DEFAULT)
        {
            c->is_default = c == button;
        }
        else
        {
            c->is_cancel = c == button;
        }
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
        case TF_PROP_VALUE:
            if (control->type == TF_CTL_OPTIONBUTTON)
            {
                if (value != 0)
                {
                    select_option(control);
                }
                else
                {
                    control->value = 0;
                }
            }
            else
            {
                /* CheckBox: 0, 1 or 2 (VB's vbGrayed), else error 380. */
                if (value < 0 || value > 2)
                {
                    tf_raise(TF_ERR_INVALID_PROPERTY, NULL);
                    break;
                }
                const int was = control->value;
                control->value = value;
                if (was != value)
                {
                    fire(control, TF_EVENT_CLICK);
                }
            }
            break;
        case TF_PROP_DEFAULT:
            control->is_default = value != 0;
            if (value != 0)
            {
                make_only(control, prop);
            }
            break;
        case TF_PROP_CANCEL:
            control->is_cancel = value != 0;
            if (value != 0)
            {
                make_only(control, prop);
            }
            break;
        case TF_PROP_ENABLED:
            control->enabled = value != 0;
            break;
        case TF_PROP_VISIBLE:
            control->visible = value != 0;
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
        case TF_PROP_VALUE:
            return control->value;
        case TF_PROP_DEFAULT:
            return control->is_default;
        case TF_PROP_CANCEL:
            return control->is_cancel;
        case TF_PROP_ENABLED:
            return control->enabled;
        case TF_PROP_VISIBLE:
            return control->visible;
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

/* A control is shown, and usable, only if every container it is in is. */
static int shown(const tf_control *c)
{
    for (; c->type != TF_CTL_FORM; c = c->parent)
    {
        if (!c->visible)
        {
            return 0;
        }
    }
    return 1;
}

static int usable(const tf_control *c)
{
    for (; c->type != TF_CTL_FORM; c = c->parent)
    {
        if (!c->enabled || !c->visible)
        {
            return 0;
        }
    }
    return 1;
}

/* The access key of a caption: the character after a single &, lower-case
   (ASCII), or 0. */
static uint32_t access_key(const tf_string *caption)
{
    const char *p = tf_str_cstr(caption);
    for (; *p != '\0'; ++p)
    {
        if (*p == '&')
        {
            if (p[1] == '&')
            {
                ++p;
                continue;
            }
            const unsigned char c = (unsigned char)p[1];
            return c >= 'A' && c <= 'Z' ? (uint32_t)(c - 'A' + 'a') : c;
        }
    }
    return 0;
}

/* Draws a caption, its access key (after &) in hot; "&&" is an &. Returns
   the cells used. */
static int draw_caption(int x, int y, const tf_string *caption, int max_cells, int fg, int bg,
                        int hot)
{
    const char *p = tf_str_cstr(caption);
    int cells = 0;
    int next_hot = 0;
    while (*p != '\0' && cells < max_cells)
    {
        if (*p == '&' && p[1] != '&' && !next_hot)
        {
            next_hot = 1;
            ++p;
            continue;
        }
        if (*p == '&' && p[1] == '&')
        {
            ++p;
        }
        uint32_t cp = 0;
        p += tf_utf8_decode(p, (int)strlen(p), &cp);
        tf_screen_put(x + cells, y, cp, next_hot ? hot : fg, bg);
        next_hot = 0;
        ++cells;
    }
    return cells;
}

static void draw_button(const tf_control *button, int x, int y, int max_width, int focused)
{
    const int w = button->width < max_width ? (int)button->width : max_width;
    if (w <= 0)
    {
        return;
    }
    const int on = usable(button);
    const int fg = on ? TF_BLACK : TF_BRIGHT + TF_BLACK;
    const int bg = focused ? TF_CYAN : TF_GREY;

    char caption[256];
    strip_accelerator(tf_str_cstr(button->caption), caption, sizeof caption);
    /* "< OK >"; a Default button's brackets stand out. */
    const int cells = utf8_cells(caption) + 4;
    const int pad = cells < w ? (w - cells) / 2 : 0;
    tf_screen_fill(x, y, w, 1, ' ', fg, bg);
    const int bracket = button->is_default && on ? TF_WHITE : fg;
    if (w - pad > 0)
    {
        tf_screen_put(x + pad, y, '<', bracket, bg);
    }
    const int used =
        draw_caption(x + pad + 2, y, button->caption, w - pad - 4, fg, bg, on ? TF_WHITE : fg);
    if (pad + 2 + used + 1 < w)
    {
        tf_screen_put(x + pad + 2 + used + 1, y, '>', bracket, bg);
    }
}

static void draw_label(const tf_control *label, int x, int y, int w, int h)
{
    const int fg = usable(label) ? TF_BLACK : TF_BRIGHT + TF_BLACK;
    if (h <= 1 || utf8_cells(tf_str_cstr(label->caption)) <= w)
    {
        draw_caption(x, y, label->caption, w, fg, TF_GREY, TF_WHITE);
        return;
    }
    /* Taller than a line: words wrapped (the access key is not shown). */
    char text[1024];
    strip_accelerator(tf_str_cstr(label->caption), text, sizeof text);
    const char *p = text;
    for (int row = 0; row < h && *p != '\0'; ++row)
    {
        while (*p == ' ')
        {
            ++p;
        }
        const char *line_end = p;
        const char *last_space = NULL;
        int cells = 0;
        for (const char *q = p; *q != '\0' && cells < w; ++cells)
        {
            uint32_t cp = 0;
            q += tf_utf8_decode(q, (int)strlen(q), &cp);
            if (cp == ' ')
            {
                last_space = q - 1;
            }
            line_end = q;
        }
        if (*line_end != '\0' && *line_end != ' ' && last_space != NULL)
        {
            line_end = last_space;
        }
        char line[1024];
        const size_t n = (size_t)(line_end - p);
        memcpy(line, p, n);
        line[n] = '\0';
        tf_screen_text(x, y + row, line, w, fg, TF_GREY);
        p = line_end;
    }
}

/* "[X] Caption" and "(*) Caption"; the cursor sits in the box when focused. */
static void draw_check(const tf_control *c, int x, int y, int w, int focused)
{
    const int on = usable(c);
    const int fg = on ? TF_BLACK : TF_BRIGHT + TF_BLACK;
    uint32_t mark = ' ';
    if (c->type == TF_CTL_CHECKBOX)
    {
        mark = c->value == 1 ? 'X' : c->value == 2 ? '-' : ' ';
    }
    else if (c->value)
    {
        mark = box(0x2022, '*'); /* a bullet */
    }
    const int round = c->type == TF_CTL_OPTIONBUTTON;
    if (w >= 3)
    {
        tf_screen_put(x, y, round ? '(' : '[', fg, TF_GREY);
        tf_screen_put(x + 1, y, mark, fg, TF_GREY);
        tf_screen_put(x + 2, y, round ? ')' : ']', fg, TF_GREY);
    }
    if (w > 4)
    {
        draw_caption(x + 4, y, c->caption, w - 4, fg, TF_GREY, on ? TF_WHITE : fg);
    }
    if (focused)
    {
        tf_screen_cursor(x + 1, y, 1);
    }
}

static void draw_frame(const tf_control *frame, int x, int y, int w, int h)
{
    const int fg = usable(frame) ? TF_BLACK : TF_BRIGHT + TF_BLACK;
    const int right = x + (int)frame->width - 1;
    const int bottom = y + (int)frame->height - 1;
    for (int i = x; i <= right && i < x + w; ++i)
    {
        tf_screen_put(i, y, box(0x2500, '-'), fg, TF_GREY);
        if (bottom < y + h)
        {
            tf_screen_put(i, bottom, box(0x2500, '-'), fg, TF_GREY);
        }
    }
    for (int j = y; j <= bottom && j < y + h; ++j)
    {
        tf_screen_put(x, j, box(0x2502, '|'), fg, TF_GREY);
        if (right < x + w)
        {
            tf_screen_put(right, j, box(0x2502, '|'), fg, TF_GREY);
        }
    }
    tf_screen_put(x, y, box(0x250C, '+'), fg, TF_GREY);
    if (right < x + w)
    {
        tf_screen_put(right, y, box(0x2510, '+'), fg, TF_GREY);
    }
    if (bottom < y + h)
    {
        tf_screen_put(x, bottom, box(0x2514, '+'), fg, TF_GREY);
        if (right < x + w)
        {
            tf_screen_put(right, bottom, box(0x2518, '+'), fg, TF_GREY);
        }
    }
    if (tf_str_lenb(frame->caption) > 0 && w > 4)
    {
        tf_screen_put(x + 1, y, ' ', fg, TF_GREY);
        const int used =
            draw_caption(x + 2, y, frame->caption, (int)frame->width - 4, fg, TF_GREY, TF_WHITE);
        tf_screen_put(x + 2 + used, y, ' ', fg, TF_GREY);
    }
}

/* Where a container's inside begins on the screen, and how much of it is
   visible (a Frame's inside, cut to what its own container shows). */
typedef struct area
{
    int x, y, w, h;
} area;

static area form_area; /* the running form's client area, set by draw_form */

static area inside(const tf_control *container)
{
    if (container->type == TF_CTL_FORM)
    {
        return form_area;
    }
    const area outer = inside(container->parent);
    area a = {outer.x + (int)container->left + 1, outer.y + (int)container->top + 1,
              (int)container->width - 2, (int)container->height - 2};
    const int right = outer.x + outer.w;
    const int bottom = outer.y + outer.h;
    a.w = a.x + a.w > right ? right - a.x : a.w;
    a.h = a.y + a.h > bottom ? bottom - a.y : a.h;
    return a;
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

    /* Controls, in tab order (a Frame before what it holds), each placed in
       its container and cut to what that shows. */
    form_area = (area){ox + 1, oy + 1, fw - 2, fh - 2};
    for (int i = 0; i < form->child_count; ++i)
    {
        tf_control *c = form->children[i];
        if (!shown(c))
        {
            continue;
        }
        const area a = inside(c->parent);
        if (c->left < 0 || c->top < 0 || c->left >= a.w || c->top >= a.h)
        {
            continue;
        }
        const int x = a.x + (int)c->left;
        const int y = a.y + (int)c->top;
        const int room = a.w - (int)c->left;
        const int rows_left = a.h - (int)c->top;
        const int focused = i == form->focus;
        const int h = (int)c->height < rows_left ? (int)c->height : rows_left;
        const int w = (int)c->width < room ? (int)c->width : room;
        switch (c->type)
        {
            case TF_CTL_TEXTBOX:
                draw_textbox(c, x, y, room, focused);
                break;
            case TF_CTL_COMMANDBUTTON:
                draw_button(c, x, y, room, focused);
                break;
            case TF_CTL_LABEL:
                draw_label(c, x, y, w, h);
                break;
            case TF_CTL_CHECKBOX:
            case TF_CTL_OPTIONBUTTON:
                draw_check(c, x, y, w, focused);
                break;
            case TF_CTL_FRAME:
                draw_frame(c, x, y, room, rows_left);
                break;
            default:
                break;
        }
    }
}

/* ------------------------------------------------------------------------ */
/* Keyboard handling                                                        */
/* ------------------------------------------------------------------------ */

static int focusable(const tf_control *c)
{
    return (c->type == TF_CTL_TEXTBOX || c->type == TF_CTL_COMMANDBUTTON ||
            c->type == TF_CTL_CHECKBOX || c->type == TF_CTL_OPTIONBUTTON) &&
           usable(c);
}

/* Tab stops once in an OptionButton group: at its selected button, or the
   first one if none is selected (as VB). */
static int tab_stop(const tf_control *form, int index)
{
    const tf_control *c = form->children[index];
    if (!focusable(c))
    {
        return 0;
    }
    if (c->type != TF_CTL_OPTIONBUTTON || c->value)
    {
        return 1;
    }
    for (int i = 0; i < form->child_count; ++i)
    {
        const tf_control *o = form->children[i];
        if (o->type == TF_CTL_OPTIONBUTTON && o->parent == c->parent && focusable(o) &&
            (o->value || i < index))
        {
            return 0; /* the group's stop is another button */
        }
    }
    return 1;
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
        if (tab_stop(form, index))
        {
            form->focus = index;
            return;
        }
    }
}

/* An arrow in an OptionButton group: the next button of the group (round
   the end), focused and selected. */
static void move_in_group(tf_control *form, int step)
{
    const tf_control *from = form->children[form->focus];
    int index = form->focus;
    for (int tries = 0; tries < form->child_count; ++tries)
    {
        index = (index + step + form->child_count) % form->child_count;
        tf_control *c = form->children[index];
        if (c->type == TF_CTL_OPTIONBUTTON && c->parent == from->parent && focusable(c))
        {
            form->focus = index;
            select_option(c);
            return;
        }
    }
}

/* The form's Default (or Cancel) button, if it can be pressed. */
static tf_control *special_button(const tf_control *form, int cancel)
{
    for (int i = 0; i < form->child_count; ++i)
    {
        tf_control *c = form->children[i];
        if (c->type == TF_CTL_COMMANDBUTTON && (cancel ? c->is_cancel : c->is_default) && usable(c))
        {
            return c;
        }
    }
    return NULL;
}

/* Alt+letter: the control whose caption has &letter. A button is pressed,
   a CheckBox toggled, an OptionButton selected; a Label or Frame passes the
   focus to the next control that can take it, as VB. Returns 1 if found. */
static void toggle(tf_control *check);

static int access(tf_control *form, uint32_t letter)
{
    for (int i = 0; i < form->child_count; ++i)
    {
        tf_control *c = form->children[i];
        if (access_key(c->caption) != letter || !usable(c))
        {
            continue;
        }
        switch (c->type)
        {
            case TF_CTL_COMMANDBUTTON:
                form->focus = i;
                fire(c, TF_EVENT_CLICK);
                return 1;
            case TF_CTL_CHECKBOX:
                form->focus = i;
                toggle(c);
                return 1;
            case TF_CTL_OPTIONBUTTON:
                form->focus = i;
                select_option(c);
                return 1;
            case TF_CTL_LABEL:
            case TF_CTL_FRAME:
                for (int j = i + 1; j < form->child_count; ++j)
                {
                    if (focusable(form->children[j]))
                    {
                        form->focus = j;
                        return 1;
                    }
                }
                return 1;
            default:
                break;
        }
    }
    return 0;
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

static void toggle(tf_control *check)
{
    check->value = check->value == 1 ? 0 : 1; /* grey (2) becomes clear */
    fire(check, TF_EVENT_CLICK);
}

static int is_space(tf_key key)
{
    return key.kind == TF_KEY_CHAR && key.ch == ' ';
}

/* A key for the focused control; 0 if it does not use it (so Enter can
   press the Default button). */
static int control_key(tf_control *form, tf_control *c, tf_key key)
{
    switch (c->type)
    {
        case TF_CTL_TEXTBOX:
            /* Only what editing uses: Enter and Esc go on to the Default and
               Cancel buttons. */
            switch (key.kind)
            {
                case TF_KEY_CHAR:
                case TF_KEY_BACKSPACE:
                case TF_KEY_DELETE:
                case TF_KEY_LEFT:
                case TF_KEY_RIGHT:
                case TF_KEY_HOME:
                case TF_KEY_END:
                    textbox_key(c, key);
                    return 1;
                default:
                    return 0;
            }
        case TF_CTL_COMMANDBUTTON:
            if (key.kind == TF_KEY_ENTER || is_space(key))
            {
                fire(c, TF_EVENT_CLICK);
                return 1;
            }
            return 0;
        case TF_CTL_CHECKBOX:
            if (is_space(key))
            {
                toggle(c);
                return 1;
            }
            return 0;
        case TF_CTL_OPTIONBUTTON:
            if (is_space(key))
            {
                select_option(c);
                return 1;
            }
            if (key.kind == TF_KEY_DOWN || key.kind == TF_KEY_RIGHT)
            {
                move_in_group(form, 1);
                return 1;
            }
            if (key.kind == TF_KEY_UP || key.kind == TF_KEY_LEFT)
            {
                move_in_group(form, -1);
                return 1;
            }
            return 0;
        default:
            return 0;
    }
}

/* One key for the form. Returns 1 if the form is to close. */
static int form_key(tf_control *form, tf_key key)
{
    if (key.kind == TF_KEY_F && key.ch == 4 && (key.mods & (TF_MOD_CTRL | TF_MOD_ALT)) != 0)
    {
        return 1; /* Ctrl+F4 or Alt+F4 */
    }
    if (form->focus < 0 || !focusable(form->children[form->focus]))
    {
        move_focus(form, 1); /* the focused control was hidden or disabled */
    }
    if (key.kind == TF_KEY_TAB || key.kind == TF_KEY_BACKTAB)
    {
        move_focus(form, key.kind == TF_KEY_TAB ? 1 : -1);
        return 0;
    }
    if (key.kind == TF_KEY_ALT)
    {
        access(form, key.ch);
        return 0;
    }
    if (form->focus >= 0 && control_key(form, form->children[form->focus], key))
    {
        return 0;
    }
    if (key.kind == TF_KEY_ENTER || key.kind == TF_KEY_ESC)
    {
        tf_control *button = special_button(form, key.kind == TF_KEY_ESC);
        if (button != NULL)
        {
            fire(button, TF_EVENT_CLICK);
        }
    }
    return 0;
}

/* ------------------------------------------------------------------------ */
/* Showing the form                                                         */
/* ------------------------------------------------------------------------ */

static int headless = 0;
static tf_control *running_form = NULL; /* the form tf_form_run is showing */

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

/* While Shell runs a command: the terminal back to normal for it, then
   taken again and redrawn whole (the command may have left anything on
   the screen, and the window may have been resized). */
static void ui_suspend(void)
{
    tf_term_close();
}

static void ui_resume(void)
{
    const char *reason = "";
    if (tf_term_open(&reason) != 0)
    {
        tf_fatal_error(TF_ERR_TERMINAL, reason);
    }
    int cols = 80;
    int rows = 25;
    tf_term_size(&cols, &rows);
    tf_screen_init(cols, rows, 1);
    if (running_form != NULL)
    {
        draw_form(running_form);
        tf_screen_flush();
    }
}

/* Registered as the shutdown hook: runs on normal close, End, or an error. */
static void ui_shutdown(void)
{
    if (headless)
    {
        /* Draw first: an error or End may come before the next redraw,
           and the dump should show what the program last set. */
        if (running_form != NULL)
        {
            draw_form(running_form);
        }
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
            tf_fatal_error(TF_ERR_TERMINAL, reason);
        }
        use_utf8 = tf_term_utf8();
        tf_term_size(&cols, &rows);
        tf_screen_init(cols, rows, 1);
        tf_input_start(NULL);
        tf_set_terminal_hooks(ui_suspend, ui_resume);
    }
    tf_set_shutdown_hook(ui_shutdown);
}

int tf_form_run(tf_control *form)
{
    running_form = form;
    ui_open();
    move_focus(form, 1);

    for (;;)
    {
        draw_form(form);
        tf_screen_flush();

        const tf_key key = tf_input_next();
        if (key.kind == TF_KEY_EOF)
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
        if (form_key(form, key))
        {
            break;
        }
    }

    /* Draw the final state so a headless dump shows what the user last saw. */
    draw_form(form);
    tf_run_shutdown_hook();
    return 0;
}
