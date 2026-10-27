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
 *
 * Menus (as VB-DOS): a form with Menus has a menu bar on its first row
 * inside the border, and its client area starts below it. F10 (or Ctrl+B,
 * for keyboards whose function keys the system takes) moves to the bar;
 * Alt+letter opens a menu straight away, before any control's access key.
 * An item's Shortcut works whenever the form is on top. Opening a menu
 * fires its Click (so the program can update its items first), as VB.
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
    tf_ref_handler ref_handlers[TF_EVENT_COUNT]; /* Unload, KeyPress */
    tf_key_handler key_handler;                  /* KeyDown */

    /* Form only: children in tab order, and the focused child's index. */
    tf_control **children;
    int child_count;
    int child_capacity;
    int focus;
    int loaded;    /* its Load event has run (and no Unload since) */
    int is_dialog; /* a MsgBox or InputBox */
    int escapable; /* a dialog: may Ctrl+F4 close it (it has a Cancel answer)? */

    /* TextBox only: cursor as a byte offset into text, and the index of the
     * first visible code point. */
    int32_t cursor;
    int32_t scroll;

    /* ListBox only: the items, which are selected, the current item
     * (ListIndex), the first shown (TopIndex), MultiSelect and Sorted. */
    tf_string **items;
    unsigned char *selected;
    int32_t item_count;
    int32_t item_capacity;
    int32_t list_index;
    int32_t top_index;
    int32_t multi;
    int sorted;

    /* Timer only: milliseconds between events (0: stopped), and when the
     * next is due on the runtime's clock (now_ms). */
    int32_t interval;
    int64_t due;

    /* Menu only: the check mark, and the Shortcut (its text in `text`)
     * as the key it is (kind TF_KEY_NONE for none). */
    int checked;
    tf_key shortcut;

    /* Form only, while the menu bar is in use: the bar menu highlighted
     * (an index into children), whether its drop-down is open, and the
     * item highlighted there (-1 none). */
    int menu_on;
    int menu_top;
    int menu_down;
    int menu_item;
};

static int headless = 0;
static int64_t virtual_now = 0; /* a key script's clock: moved only by {WAIT n} */

/* The forms shown, bottom first: the main form, then forms and dialogs
   shown over it. Only the top one takes keys. */
#define TF_STACK_MAX 32
static tf_control *stack[TF_STACK_MAX];
static int depth = 0;

/* Milliseconds on the clock Timers run by: real time, or a key script's. */
static int64_t now_ms(void)
{
    return headless ? virtual_now : tf_platform_monotonic_ms();
}

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
    const int boxed = type == TF_CTL_FRAME || type == TF_CTL_LISTBOX;
    control->width = type == TF_CTL_TEXTBOX || boxed ? 20 : 10;
    control->height = boxed ? 5 : 1;
    control->list_index = -1;
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

void tf_control_on_ref(tf_control *control, tf_event event, tf_ref_handler handler)
{
    control->ref_handlers[event] = handler;
}

void tf_control_on_key(tf_control *control, tf_key_handler handler)
{
    control->key_handler = handler;
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

/* Events with arguments (Unload, KeyPress; KeyDown). */
static void fire_ref(tf_control *control, tf_event event, int32_t *value)
{
    if (control->ref_handlers[event] != NULL)
    {
        control->ref_handlers[event](value);
        tf_check_uncaught();
    }
}

static void fire_key(tf_control *control, int32_t *code, int32_t shift)
{
    if (control->key_handler != NULL)
    {
        control->key_handler(code, shift);
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

/* Makes item the ListBox's current one (ListIndex), shown; with only (one
   item at a time) it is the selection too. Click fires on a change. */
static void list_choose(tf_control *list, int32_t item, int only)
{
    const int was = list->list_index;
    int changed = was != item;
    list->list_index = item;
    if (only)
    {
        for (int32_t i = 0; i < list->item_count; ++i)
        {
            const unsigned char want = i == item;
            changed = changed || list->selected[i] != want;
            list->selected[i] = want;
        }
    }
    const int32_t rows = list->height - 2 > 0 ? list->height - 2 : 1;
    if (item >= 0 && item < list->top_index)
    {
        list->top_index = item;
    }
    else if (item >= list->top_index + rows)
    {
        list->top_index = item - rows + 1;
    }
    if (changed)
    {
        fire(list, TF_EVENT_CLICK);
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

static int parse_shortcut(const char *text, tf_key *key);

void tf_set_str(tf_control *control, tf_prop prop, const tf_string *value)
{
    /* Retain before releasing, in case value is the current string. */
    tf_string *v = (tf_string *)value;
    if (prop == TF_PROP_SHORTCUT)
    {
        tf_key key;
        if (!parse_shortcut(tf_str_cstr(v), &key))
        {
            tf_raise(TF_ERR_INVALID_PROPERTY, NULL);
            return;
        }
        /* Shown as Windows writes it, however it was typed. */
        char shown_as[32] = "";
        if (key.kind != TF_KEY_NONE)
        {
            const int mods = key.kind == TF_KEY_CTRL ? TF_MOD_CTRL : key.mods;
            snprintf(shown_as, sizeof shown_as, "%s%s%s", (mods & TF_MOD_CTRL) ? "Ctrl+" : "",
                     (mods & TF_MOD_ALT) ? "Alt+" : "", (mods & TF_MOD_SHIFT) ? "Shift+" : "");
            const size_t used = strlen(shown_as);
            if (key.kind == TF_KEY_CTRL)
            {
                snprintf(shown_as + used, sizeof shown_as - used, "%c", (char)(key.ch - 'a' + 'A'));
            }
            else if (key.kind == TF_KEY_F)
            {
                snprintf(shown_as + used, sizeof shown_as - used, "F%u", (unsigned)key.ch);
            }
            else
            {
                snprintf(shown_as + used, sizeof shown_as - used, "Del");
            }
        }
        control->shortcut = key;
        tf_str_assign(&control->text, tf_str_from_utf8(shown_as, (int32_t)strlen(shown_as)));
        return;
    }
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
    if (control->type == TF_CTL_LISTBOX && prop == TF_PROP_TEXT)
    {
        /* The current item's text (VB's Text of a ListBox). */
        const int32_t i = control->list_index;
        return i >= 0 && i < control->item_count ? control->items[i] : NULL;
    }
    return prop == TF_PROP_TEXT || prop == TF_PROP_SHORTCUT ? control->text : control->caption;
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
            control->due = now_ms() + control->interval; /* a Timer starts counting again */
            break;
        case TF_PROP_INTERVAL:
            control->interval = value < 0 ? 0 : value;
            control->due = now_ms() + control->interval;
            break;
        case TF_PROP_VISIBLE:
            control->visible = value != 0;
            break;
        case TF_PROP_LISTINDEX:
            if (value < -1 || value >= control->item_count)
            {
                tf_raise(TF_ERR_INVALID_PROPERTY, NULL);
                break;
            }
            list_choose(control, value, control->multi == 0);
            break;
        case TF_PROP_MULTISELECT:
            control->multi = value < 0 ? 0 : value > 2 ? 2 : value;
            break;
        case TF_PROP_SORTED:
            control->sorted = value != 0;
            break;
        case TF_PROP_TOPINDEX:
            control->top_index = value < 0 ? 0 : value;
            break;
        case TF_PROP_CHECKED:
            control->checked = value != 0;
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
        case TF_PROP_LISTCOUNT:
            return control->item_count;
        case TF_PROP_LISTINDEX:
            return control->list_index;
        case TF_PROP_MULTISELECT:
            return control->multi;
        case TF_PROP_SORTED:
            return control->sorted;
        case TF_PROP_SELCOUNT:
        {
            int32_t n = 0;
            for (int32_t i = 0; i < control->item_count; ++i)
            {
                n += control->selected[i] != 0;
            }
            return n;
        }
        case TF_PROP_TOPINDEX:
            return control->top_index;
        case TF_PROP_INTERVAL:
            return control->interval;
        case TF_PROP_CHECKED:
            return control->checked;
        default:
            return 0;
    }
}

/* ------------------------------------------------------------------------ */
/* ListBox items                                                            */
/* ------------------------------------------------------------------------ */

/* Compares for Sorted: A-Z case ignored, then byte order. */
static int item_order(const tf_string *a, const tf_string *b)
{
    const char *x = tf_str_cstr(a);
    const char *y = tf_str_cstr(b);
    for (;; ++x, ++y)
    {
        int cx = (unsigned char)*x;
        int cy = (unsigned char)*y;
        cx = cx >= 'A' && cx <= 'Z' ? cx - 'A' + 'a' : cx;
        cy = cy >= 'A' && cy <= 'Z' ? cy - 'A' + 'a' : cy;
        if (cx != cy || cx == 0)
        {
            return cx - cy;
        }
    }
}

void tf_list_add(tf_control *list, const tf_string *item, int32_t index)
{
    if (index < -1 || index > list->item_count)
    {
        tf_raise(TF_ERR_INVALID_ARGUMENT, NULL);
        return;
    }
    if (index == -1)
    {
        index = list->item_count;
        if (list->sorted)
        {
            index = 0;
            while (index < list->item_count && item_order(list->items[index], item) <= 0)
            {
                ++index;
            }
        }
    }
    if (list->item_count == list->item_capacity)
    {
        list->item_capacity = list->item_capacity == 0 ? 16 : list->item_capacity * 2;
        list->items = tf_xrealloc(list->items, (size_t)list->item_capacity * sizeof *list->items);
        list->selected = tf_xrealloc(list->selected, (size_t)list->item_capacity);
    }
    const size_t after = (size_t)(list->item_count - index);
    memmove(&list->items[index + 1], &list->items[index], after * sizeof *list->items);
    memmove(&list->selected[index + 1], &list->selected[index], after);
    list->items[index] = (tf_string *)item;
    tf_str_retain(list->items[index]);
    list->selected[index] = 0;
    ++list->item_count;
    if (list->list_index >= index)
    {
        ++list->list_index; /* the current item keeps its place */
    }
}

void tf_list_remove(tf_control *list, int32_t index)
{
    if (index < 0 || index >= list->item_count)
    {
        tf_raise(TF_ERR_INVALID_ARGUMENT, NULL);
        return;
    }
    tf_str_release(list->items[index]);
    const size_t after = (size_t)(list->item_count - index - 1);
    memmove(&list->items[index], &list->items[index + 1], after * sizeof *list->items);
    memmove(&list->selected[index], &list->selected[index + 1], after);
    --list->item_count;
    if (list->list_index == index)
    {
        list->list_index = -1; /* as VB: no current item */
    }
    else if (list->list_index > index)
    {
        --list->list_index;
    }
}

void tf_list_clear(tf_control *list)
{
    for (int32_t i = 0; i < list->item_count; ++i)
    {
        tf_str_release(list->items[i]);
    }
    list->item_count = 0;
    list->list_index = -1;
    list->top_index = 0;
}

tf_string *tf_list_item(const tf_control *list, int32_t index)
{
    if (index < 0 || index >= list->item_count)
    {
        tf_raise(TF_ERR_INVALID_ARGUMENT, NULL);
        return NULL;
    }
    return tf_str_ref(list->items[index]);
}

void tf_list_set_item(tf_control *list, int32_t index, const tf_string *item)
{
    if (index < 0 || index >= list->item_count)
    {
        tf_raise(TF_ERR_INVALID_ARGUMENT, NULL);
        return;
    }
    tf_string *v = (tf_string *)item;
    tf_str_retain(v);
    tf_str_assign(&list->items[index], v);
}

int tf_list_selected(const tf_control *list, int32_t index)
{
    if (index < 0 || index >= list->item_count)
    {
        tf_raise(TF_ERR_INVALID_ARGUMENT, NULL);
        return 0;
    }
    return list->multi == 0 ? index == list->list_index : list->selected[index] != 0;
}

void tf_list_set_selected(tf_control *list, int32_t index, int selected)
{
    if (index < 0 || index >= list->item_count)
    {
        tf_raise(TF_ERR_INVALID_ARGUMENT, NULL);
        return;
    }
    if (list->multi == 0)
    {
        if (selected)
        {
            list_choose(list, index, 1);
        }
        else if (index == list->list_index)
        {
            list_choose(list, -1, 1);
        }
        return;
    }
    const int was = list->selected[index];
    list->selected[index] = selected != 0;
    if (was != list->selected[index])
    {
        fire(list, TF_EVENT_CLICK);
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

static void draw_one(tf_control *form);

/* The backdrop and every form on the stack; or, if the terminal is too
   small for one of them, a message saying so. */
static void draw_stack(void)
{
    int cols = 0;
    int rows = 0;
    tf_screen_size(&cols, &rows);
    tf_screen_cursor(0, 0, 0);
    for (int i = 0; i < depth; ++i)
    {
        const int fw = (int)stack[i]->width;
        const int fh = (int)stack[i]->height;
        if (cols < fw || rows < fh)
        {
            char message[96];
            snprintf(message, sizeof message, "Please resize the terminal to at least %dx%d", fw,
                     fh);
            tf_screen_fill(0, 0, cols, rows, ' ', TF_GREY, TF_BLACK);
            const int length = (int)strlen(message);
            const int x = length < cols ? (cols - length) / 2 : 0;
            tf_screen_text(x, rows / 2, message, cols, TF_WHITE, TF_BLACK);
            return;
        }
    }
    tf_screen_fill(0, 0, cols, rows, ' ', TF_GREY, TF_BLUE);
    for (int i = 0; i < depth; ++i)
    {
        draw_one(stack[i]);
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

static void draw_frame(const tf_control *frame, int x, int y, int w, int h);

/* A box of items; selected ones highlighted, the current one under the
   cursor when focused, arrows in the border when there is more. */
static void draw_list(tf_control *list, int x, int y, int w, int h, int focused)
{
    draw_frame(list, x, y, w, h);
    const int on = usable(list);
    const int rows = (int)list->height - 2;
    const int inner = (int)list->width - 2;
    if (list->top_index > list->item_count - rows && list->item_count > rows)
    {
        list->top_index = list->item_count - rows;
    }
    for (int r = 0; r < rows && r + 1 < h; ++r)
    {
        const int32_t i = list->top_index + r;
        int fg = on ? TF_BLACK : TF_BRIGHT + TF_BLACK;
        int bg = TF_GREY;
        if (i < list->item_count && list->selected[i])
        {
            fg = TF_WHITE;
            bg = focused ? TF_BLUE : TF_BRIGHT + TF_BLACK;
        }
        const int cells = inner < w - 1 ? inner : w - 1;
        tf_screen_fill(x + 1, y + 1 + r, cells, 1, ' ', fg, bg);
        if (i < list->item_count)
        {
            tf_screen_text(x + 1, y + 1 + r, tf_str_cstr(list->items[i]), cells, fg, bg);
        }
    }
    const int right = x + (int)list->width - 1;
    if (right < x + w)
    {
        if (list->top_index > 0)
        {
            tf_screen_put(right, y + 1, box(0x2191, '^'), TF_BLACK, TF_GREY);
        }
        if (list->top_index + rows < list->item_count && rows > 0)
        {
            tf_screen_put(right, y + rows, box(0x2193, 'v'), TF_BLACK, TF_GREY);
        }
    }
    const int32_t current = list->list_index;
    if (focused && current >= list->top_index && current < list->top_index + rows)
    {
        tf_screen_cursor(x + 1, y + 1 + (int)(current - list->top_index), 1);
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

static area form_area; /* the client area of the form being drawn (draw_one) */

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

/* ------------------------------------------------------------------------ */
/* Menus                                                                    */
/* ------------------------------------------------------------------------ */

static int lower_ascii(int c)
{
    return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c;
}

/* Whether text starts with prefix, ASCII case ignored. */
static int starts_with(const char *text, const char *prefix)
{
    for (; *prefix != '\0'; ++text, ++prefix)
    {
        if (lower_ascii((unsigned char)*text) != lower_ascii((unsigned char)*prefix))
        {
            return 0;
        }
    }
    return 1;
}

/* A Shortcut's text as the key it names: Ctrl+letter, F1-F12 with Shift+,
   Ctrl+ or Alt+ before it, or Del; "" is none. Returns 0 for anything else
   or a key the runtime keeps for itself (the compiler's shortcutProblem
   says why, for the form block). */
static int parse_shortcut(const char *text, tf_key *key)
{
    key->kind = TF_KEY_NONE;
    key->ch = 0;
    key->mods = 0;
    if (*text == '\0')
    {
        return 1;
    }
    for (int more = 1; more;)
    {
        more = 0;
        static const struct
        {
            const char *prefix;
            int mod;
        } kPrefixes[] = {{"Shift+", TF_MOD_SHIFT}, {"Ctrl+", TF_MOD_CTRL}, {"Alt+", TF_MOD_ALT}};
        for (size_t i = 0; i < sizeof kPrefixes / sizeof kPrefixes[0]; ++i)
        {
            const size_t n = strlen(kPrefixes[i].prefix);
            if (strlen(text) > n && starts_with(text, kPrefixes[i].prefix))
            {
                key->mods |= kPrefixes[i].mod;
                text += n;
                more = 1;
            }
        }
    }
    const int letter = lower_ascii((unsigned char)text[0]);
    if (letter >= 'a' && letter <= 'z' && text[1] == '\0')
    {
        /* Ctrl+H, I and M are Backspace, Tab and Enter; Ctrl+B is the bar. */
        if (key->mods != TF_MOD_CTRL || letter == 'h' || letter == 'i' || letter == 'm' ||
            letter == 'b')
        {
            return 0;
        }
        key->kind = TF_KEY_CTRL;
        key->ch = (uint32_t)letter;
        key->mods = 0;
        return 1;
    }
    if ((starts_with(text, "Del") && text[3] == '\0') ||
        (starts_with(text, "Delete") && text[6] == '\0'))
    {
        key->kind = TF_KEY_DELETE;
        return key->mods == 0;
    }
    if (letter == 'f' && text[1] >= '1' && text[1] <= '9' &&
        (text[2] == '\0' || (text[2] >= '0' && text[2] <= '9' && text[3] == '\0')))
    {
        const int n = text[2] == '\0' ? text[1] - '0' : (text[1] - '0') * 10 + text[2] - '0';
        if (n > 12 || (n == 10 && key->mods == 0) ||
            (n == 4 && (key->mods & (TF_MOD_CTRL | TF_MOD_ALT)) != 0))
        {
            return 0; /* F10 opens the bar; Ctrl+F4 and Alt+F4 close the form */
        }
        key->kind = TF_KEY_F;
        key->ch = (uint32_t)n;
        return 1;
    }
    return 0;
}

static int is_menu_bar_key(tf_key key)
{
    return (key.kind == TF_KEY_F && key.ch == 10 && key.mods == 0) ||
           (key.kind == TF_KEY_CTRL && key.ch == 'b');
}

static int on_bar(const tf_control *form, const tf_control *c)
{
    return c->type == TF_CTL_MENU && c->parent == form;
}

static int has_menu_bar(const tf_control *form)
{
    for (int i = 0; i < form->child_count; ++i)
    {
        if (on_bar(form, form->children[i]))
        {
            return 1;
        }
    }
    return 0;
}

static int is_separator(const tf_control *item)
{
    return strcmp(tf_str_cstr(item->caption), "-") == 0;
}

/* The next bar menu after index from (-1: the first) in the direction step,
   round the ends, that can be opened; -1 if none can. */
static int next_on_bar(const tf_control *form, int from, int step)
{
    const int n = form->child_count;
    int i = from < 0 ? (step > 0 ? -1 : n) : from;
    for (int tried = 0; tried < n; ++tried)
    {
        i = (i + step + n) % n;
        if (on_bar(form, form->children[i]) && usable(form->children[i]))
        {
            return i;
        }
    }
    return -1;
}

/* The same, among the items of the bar menu at index top that can be
   chosen (separators and disabled items are passed over). */
static int next_item(const tf_control *form, int top, int from, int step)
{
    const int n = form->child_count;
    int i = from < 0 ? (step > 0 ? -1 : n) : from;
    for (int tried = 0; tried < n; ++tried)
    {
        i = (i + step + n) % n;
        const tf_control *c = form->children[i];
        if (c->parent == form->children[top] && usable(c) && !is_separator(c))
        {
            return i;
        }
    }
    return -1;
}

static int has_items(const tf_control *form, int top)
{
    for (int i = 0; i < form->child_count; ++i)
    {
        if (form->children[i]->parent == form->children[top] && shown(form->children[i]))
        {
            return 1;
        }
    }
    return 0;
}

static void menu_close(tf_control *form)
{
    form->menu_on = 0;
    form->menu_down = 0;
    form->menu_item = -1;
}

/* Chooses an item, or a bar menu with no items: the menu closes first, so
   its Click handler sees the form as the user will. */
static void menu_choose(tf_control *form, int index)
{
    menu_close(form);
    fire(form->children[index], TF_EVENT_CLICK);
}

/* Opens the bar menu at index top: its Click first (VB's chance to set
   Checked and Enabled on the items), then the drop-down, on its first
   item. A bar menu without items is chosen instead. */
static void menu_open(tf_control *form, int top)
{
    if (!has_items(form, top))
    {
        menu_choose(form, top);
        return;
    }
    form->menu_on = 1;
    form->menu_top = top;
    form->menu_down = 1;
    fire(form->children[top], TF_EVENT_CLICK);
    form->menu_item = next_item(form, top, -1, 1);
}

/* The bar menu or item at index whose access key is letter, among the bar
   (parent the form) or one menu's items; -1 if none. */
static int menu_access(const tf_control *form, const tf_control *parent, uint32_t letter)
{
    for (int i = 0; i < form->child_count; ++i)
    {
        const tf_control *c = form->children[i];
        if (c->type == TF_CTL_MENU && c->parent == parent && usable(c) &&
            access_key(c->caption) == letter)
        {
            return i;
        }
    }
    return -1;
}

/* A key while the bar is in use: every key stays with the menus, except
   Ctrl+F4 (Alt+F4), which closes them and goes on to close the form.
   Returns 1 if the key was used up. */
static int menu_key(tf_control *form, tf_key key)
{
    const int top = form->menu_top;
    switch (key.kind)
    {
        case TF_KEY_ESC:
            if (form->menu_down)
            {
                form->menu_down = 0; /* back to the bar, as CUA */
            }
            else
            {
                menu_close(form);
            }
            return 1;
        case TF_KEY_LEFT:
        case TF_KEY_RIGHT:
        {
            const int next = next_on_bar(form, top, key.kind == TF_KEY_RIGHT ? 1 : -1);
            if (form->menu_down && next >= 0 && has_items(form, next))
            {
                menu_open(form, next);
            }
            else if (next >= 0)
            {
                form->menu_top = next;
                form->menu_down = 0;
            }
            return 1;
        }
        case TF_KEY_UP:
        case TF_KEY_DOWN:
            if (!form->menu_down)
            {
                if (has_items(form, top))
                {
                    menu_open(form, top);
                }
            }
            else
            {
                form->menu_item =
                    next_item(form, top, form->menu_item, key.kind == TF_KEY_DOWN ? 1 : -1);
            }
            return 1;
        case TF_KEY_ENTER:
            if (!form->menu_down)
            {
                menu_open(form, top);
            }
            else if (form->menu_item >= 0)
            {
                menu_choose(form, form->menu_item);
            }
            return 1;
        case TF_KEY_CHAR:
        case TF_KEY_ALT:
        {
            const uint32_t letter = key.ch < 0x80 ? (uint32_t)lower_ascii((int)key.ch) : key.ch;
            if (form->menu_down)
            {
                const int item = menu_access(form, form->children[top], letter);
                if (item >= 0)
                {
                    menu_choose(form, item);
                }
            }
            else
            {
                const int menu = menu_access(form, form, letter);
                if (menu >= 0)
                {
                    menu_open(form, menu);
                }
            }
            return 1;
        }
        case TF_KEY_F:
            if (key.ch == 4 && (key.mods & (TF_MOD_CTRL | TF_MOD_ALT)) != 0)
            {
                menu_close(form);
                return 0;
            }
            if (is_menu_bar_key(key))
            {
                menu_close(form);
            }
            return 1;
        case TF_KEY_CTRL:
            if (is_menu_bar_key(key))
            {
                menu_close(form);
            }
            return 1;
        default:
            return 1;
    }
}

/* A key before the focused control sees it: the menu bar key, a bar
   menu's access key (ahead of the controls', as VB) and the items'
   Shortcuts. Returns 1 if the key was used up. */
static int menu_first(tf_control *form, tf_key key)
{
    if (form->menu_on)
    {
        return menu_key(form, key);
    }
    if (!has_menu_bar(form))
    {
        return 0;
    }
    if (is_menu_bar_key(key))
    {
        form->menu_top = next_on_bar(form, -1, 1);
        form->menu_on = form->menu_top >= 0;
        form->menu_down = 0;
        form->menu_item = -1;
        return 1;
    }
    if (key.kind == TF_KEY_ALT)
    {
        const int menu = menu_access(form, form, key.ch);
        if (menu >= 0)
        {
            menu_open(form, menu);
            return 1;
        }
        return 0;
    }
    for (int i = 0; i < form->child_count; ++i)
    {
        tf_control *c = form->children[i];
        const tf_key want = c->shortcut;
        if (c->type == TF_CTL_MENU && want.kind != TF_KEY_NONE && want.kind == key.kind &&
            want.ch == key.ch && (want.kind == TF_KEY_CTRL || want.mods == key.mods) && usable(c))
        {
            fire(c, TF_EVENT_CLICK);
            return 1;
        }
    }
    return 0;
}

/* The bar menus' places: each " Caption " in turn from the bar's left. */
static int bar_x(const tf_control *form, int index, int bar_left)
{
    int x = bar_left;
    for (int i = 0; i < index; ++i)
    {
        const tf_control *c = form->children[i];
        if (on_bar(form, c) && shown(c))
        {
            char caption[256];
            strip_accelerator(tf_str_cstr(c->caption), caption, sizeof caption);
            x += utf8_cells(caption) + 2;
        }
    }
    return x;
}

static void draw_menu_bar(const tf_control *form, int x, int y, int w)
{
    tf_screen_fill(x, y, w, 1, ' ', TF_BLACK, TF_GREY);
    for (int i = 0; i < form->child_count; ++i)
    {
        const tf_control *c = form->children[i];
        if (!on_bar(form, c) || !shown(c))
        {
            continue;
        }
        const int at = bar_x(form, i, x);
        char caption[256];
        strip_accelerator(tf_str_cstr(c->caption), caption, sizeof caption);
        const int cells = utf8_cells(caption) + 2;
        if (at + cells > x + w)
        {
            break; /* the rest do not fit */
        }
        const int lit = form->menu_on && form->menu_top == i;
        const int fg = usable(c) ? TF_BLACK : TF_BRIGHT + TF_BLACK;
        const int bg = lit ? TF_CYAN : TF_GREY;
        tf_screen_fill(at, y, cells, 1, ' ', fg, bg);
        draw_caption(at + 1, y, c->caption, cells - 2, fg, bg, usable(c) ? TF_WHITE : fg);
    }
}

/* The open drop-down, under its bar menu, over whatever is below (it may
   reach past the form). Rows: check mark, caption, then the Shortcut in a
   column of its own. */
static void draw_drop_down(const tf_control *form, int bar_left, int bar_y)
{
    const tf_control *menu = form->children[form->menu_top];
    int caption_cells = 0;
    int shortcut_cells = 0;
    int rows = 0;
    for (int i = 0; i < form->child_count; ++i)
    {
        const tf_control *c = form->children[i];
        if (c->parent != menu || !shown(c))
        {
            continue;
        }
        char caption[256];
        strip_accelerator(tf_str_cstr(c->caption), caption, sizeof caption);
        const int cells = utf8_cells(caption);
        const int keys = utf8_cells(tf_str_cstr(c->text));
        caption_cells = cells > caption_cells ? cells : caption_cells;
        shortcut_cells = keys > shortcut_cells ? keys : shortcut_cells;
        ++rows;
    }
    /* "│ ✓ Caption  Shortcut │" */
    const int inner = 3 + caption_cells + (shortcut_cells > 0 ? 2 + shortcut_cells : 0) + 1;
    const int w = inner + 2;
    const int h = rows + 2;
    int cols = 0;
    int screen_rows = 0;
    tf_screen_size(&cols, &screen_rows);
    int x = bar_x(form, form->menu_top, bar_left);
    x = x + w > cols ? (cols - w < 0 ? 0 : cols - w) : x;
    const int y = bar_y + 1;

    tf_screen_fill(x, y, w, h, ' ', TF_BLACK, TF_GREY);
    for (int i = 1; i < w - 1; ++i)
    {
        tf_screen_put(x + i, y, box(0x2500, '-'), TF_BLACK, TF_GREY);
        tf_screen_put(x + i, y + h - 1, box(0x2500, '-'), TF_BLACK, TF_GREY);
    }
    for (int j = 1; j < h - 1; ++j)
    {
        tf_screen_put(x, y + j, box(0x2502, '|'), TF_BLACK, TF_GREY);
        tf_screen_put(x + w - 1, y + j, box(0x2502, '|'), TF_BLACK, TF_GREY);
    }
    tf_screen_put(x, y, box(0x250C, '+'), TF_BLACK, TF_GREY);
    tf_screen_put(x + w - 1, y, box(0x2510, '+'), TF_BLACK, TF_GREY);
    tf_screen_put(x, y + h - 1, box(0x2514, '+'), TF_BLACK, TF_GREY);
    tf_screen_put(x + w - 1, y + h - 1, box(0x2518, '+'), TF_BLACK, TF_GREY);

    int row = y + 1;
    for (int i = 0; i < form->child_count; ++i)
    {
        const tf_control *c = form->children[i];
        if (c->parent != menu || !shown(c))
        {
            continue;
        }
        if (is_separator(c))
        {
            tf_screen_put(x, row, box(0x251C, '+'), TF_BLACK, TF_GREY);
            for (int k = 1; k < w - 1; ++k)
            {
                tf_screen_put(x + k, row, box(0x2500, '-'), TF_BLACK, TF_GREY);
            }
            tf_screen_put(x + w - 1, row, box(0x2524, '+'), TF_BLACK, TF_GREY);
            ++row;
            continue;
        }
        const int on = usable(c);
        const int fg = on ? TF_BLACK : TF_BRIGHT + TF_BLACK;
        const int bg = i == form->menu_item ? TF_CYAN : TF_GREY;
        tf_screen_fill(x + 1, row, inner, 1, ' ', fg, bg);
        if (c->checked)
        {
            tf_screen_put(x + 2, row, box(0x2713, 'x'), fg, bg);
        }
        draw_caption(x + 4, row, c->caption, caption_cells, fg, bg, on ? TF_WHITE : fg);
        const char *keys = tf_str_cstr(c->text);
        const int key_cells = utf8_cells(keys);
        if (key_cells > 0)
        {
            tf_screen_text(x + 4 + caption_cells + 2, row, keys, key_cells, fg, bg);
        }
        ++row;
    }
}

/* One form, centred on the screen (the backdrop is already drawn). */
static void draw_one(tf_control *form)
{
    int cols = 0;
    int rows = 0;
    tf_screen_size(&cols, &rows);
    const int fw = (int)form->width;
    const int fh = (int)form->height;
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
    const int bar = has_menu_bar(form);
    if (bar)
    {
        draw_menu_bar(form, ox + 1, oy + 1, fw - 2);
        form_area.y++;
        form_area.h--;
    }
    const int on_top = depth > 0 && form == stack[depth - 1];
    for (int i = 0; i < form->child_count; ++i)
    {
        tf_control *c = form->children[i];
        if (!shown(c) || c->type == TF_CTL_MENU)
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
        const int focused = i == form->focus && on_top && !form->menu_on;
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
            case TF_CTL_LISTBOX:
                draw_list(c, x, y, room, rows_left, focused);
                break;
            default:
                break;
        }
    }
    if (bar && on_top && form->menu_on && form->menu_down)
    {
        draw_drop_down(form, ox + 1, oy + 1); /* last: over the controls */
    }
}

/* ------------------------------------------------------------------------ */
/* Keyboard handling                                                        */
/* ------------------------------------------------------------------------ */

static int focusable(const tf_control *c)
{
    return (c->type == TF_CTL_TEXTBOX || c->type == TF_CTL_COMMANDBUTTON ||
            c->type == TF_CTL_CHECKBOX || c->type == TF_CTL_OPTIONBUTTON ||
            c->type == TF_CTL_LISTBOX) &&
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

/* Moves the focus to child index, firing LostFocus and GotFocus. */
static void set_focus(tf_control *form, int index)
{
    if (index == form->focus)
    {
        return;
    }
    const int was = form->focus;
    form->focus = index;
    if (was >= 0 && was < form->child_count)
    {
        fire(form->children[was], TF_EVENT_LOSTFOCUS);
    }
    if (index >= 0 && form->focus == index)
    {
        fire(form->children[index], TF_EVENT_GOTFOCUS);
    }
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
            set_focus(form, index);
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
            set_focus(form, index);
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
                set_focus(form, i);
                fire(c, TF_EVENT_CLICK);
                return 1;
            case TF_CTL_CHECKBOX:
                set_focus(form, i);
                toggle(c);
                return 1;
            case TF_CTL_OPTIONBUTTON:
                set_focus(form, i);
                select_option(c);
                return 1;
            case TF_CTL_LABEL:
            case TF_CTL_FRAME:
                for (int j = i + 1; j < form->child_count; ++j)
                {
                    if (focusable(form->children[j]))
                    {
                        set_focus(form, j);
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

/* A key in a ListBox. Arrows, PgUp/PgDn, Home/End move the current item
   (selecting it, unless MultiSelect is 1); Space toggles the current item
   with MultiSelect; a letter goes to the next item starting with it; Enter
   is DblClick if the program handles it. */
static int list_key(tf_control *list, tf_key key)
{
    if (list->item_count == 0)
    {
        return 0;
    }
    const int32_t rows = list->height - 2 > 0 ? list->height - 2 : 1;
    const int32_t at = list->list_index < 0 ? -1 : list->list_index;
    int32_t to = at;
    switch (key.kind)
    {
        case TF_KEY_DOWN:
            to = at + 1;
            break;
        case TF_KEY_UP:
            to = at - 1;
            break;
        case TF_KEY_PGDN:
            to = at + rows;
            break;
        case TF_KEY_PGUP:
            to = at - rows;
            break;
        case TF_KEY_HOME:
            to = 0;
            break;
        case TF_KEY_END:
            to = list->item_count - 1;
            break;
        case TF_KEY_ENTER:
            if (list->handlers[TF_EVENT_DBLCLICK] != NULL && at >= 0)
            {
                fire(list, TF_EVENT_DBLCLICK);
                return 1;
            }
            return 0;
        case TF_KEY_CHAR:
            if (key.ch == ' ' && list->multi != 0 && at >= 0)
            {
                tf_list_set_selected(list, at, !list->selected[at]);
                return 1;
            }
            if (key.ch > ' ' && key.ch < 0x7F)
            {
                /* The next item starting with that letter, round the end. */
                const int32_t c =
                    (int32_t)(key.ch >= 'A' && key.ch <= 'Z' ? key.ch - 'A' + 'a' : key.ch);
                for (int32_t k = 1; k <= list->item_count; ++k)
                {
                    const int32_t i = (at + k + list->item_count) % list->item_count;
                    int32_t first = (unsigned char)tf_str_cstr(list->items[i])[0];
                    first = first >= 'A' && first <= 'Z' ? first - 'A' + 'a' : first;
                    if (first == c)
                    {
                        list_choose(list, i, list->multi != 1);
                        return 1;
                    }
                }
                return 1;
            }
            return 0;
        default:
            return 0;
    }
    to = to < 0 ? 0 : to >= list->item_count ? list->item_count - 1 : to;
    list_choose(list, to, list->multi != 1);
    return 1;
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
        case TF_CTL_LISTBOX:
            return list_key(c, key);
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

/* ------------------------------------------------------------------------ */
/* Timers                                                                   */
/* ------------------------------------------------------------------------ */

static int running(const tf_control *c)
{
    return c->type == TF_CTL_TIMER && c->enabled && c->interval > 0;
}

/* When the next Timer is due, or -1 if none is running. */
static int64_t next_due(const tf_control *form)
{
    int64_t next = -1;
    for (int i = 0; i < form->child_count; ++i)
    {
        const tf_control *c = form->children[i];
        if (running(c) && (next < 0 || c->due < next))
        {
            next = c->due;
        }
    }
    return next;
}

/* Fires each Timer that is due, in the order they were made. One that has
   fallen behind (a long handler) fires once and starts again from now,
   rather than catching up in a burst. */
static void fire_due(tf_control *form)
{
    const int64_t now = now_ms();
    for (int i = 0; i < form->child_count; ++i)
    {
        tf_control *c = form->children[i];
        if (running(c) && now >= c->due)
        {
            c->due += c->interval;
            if (c->due <= now)
            {
                c->due = now + c->interval;
            }
            fire(c, TF_EVENT_TIMER);
        }
    }
}

/* The Timers of every form shown run (a dialog does not stop them). */
static int64_t stack_due(void)
{
    int64_t next = -1;
    for (int i = 0; i < depth; ++i)
    {
        const int64_t due = next_due(stack[i]);
        next = due >= 0 && (next < 0 || due < next) ? due : next;
    }
    return next;
}

static void stack_fire(void)
{
    for (int i = 0; i < depth; ++i)
    {
        fire_due(stack[i]);
    }
}

/* {WAIT n} in a key script: the clock moves n ms, stopping at each due
   Timer on the way. */
static void wait_scripted(int64_t ms)
{
    const int64_t until = virtual_now + ms;
    for (int64_t next = stack_due(); next >= 0 && next <= until; next = stack_due())
    {
        virtual_now = next > virtual_now ? next : virtual_now;
        stack_fire();
    }
    virtual_now = until;
}

/* VB's KeyCode and Shift for a key (vbKeyA ..., vbShiftMask 1, vbCtrlMask
   2, vbAltMask 4); 0 for one VB has no code for. */
static int32_t key_code(tf_key key, int32_t *shift)
{
    *shift = ((key.mods & TF_MOD_SHIFT) ? 1 : 0) | ((key.mods & TF_MOD_CTRL) ? 2 : 0) |
             ((key.mods & TF_MOD_ALT) ? 4 : 0);
    switch (key.kind)
    {
        case TF_KEY_CHAR:
        case TF_KEY_CTRL:
        case TF_KEY_ALT:
        {
            *shift |= key.kind == TF_KEY_CTRL ? 2 : key.kind == TF_KEY_ALT ? 4 : 0;
            const uint32_t c = key.ch;
            if (c >= 'a' && c <= 'z')
            {
                return (int32_t)(c - 'a' + 'A');
            }
            if (c >= 'A' && c <= 'Z')
            {
                *shift |= 1;
                return (int32_t)c;
            }
            return c < 0x80 ? (int32_t)c : 0;
        }
        case TF_KEY_ENTER:
            return 13;
        case TF_KEY_TAB:
            return 9;
        case TF_KEY_BACKTAB:
            *shift |= 1;
            return 9;
        case TF_KEY_BACKSPACE:
            return 8;
        case TF_KEY_ESC:
            return 27;
        case TF_KEY_DELETE:
            return 46;
        case TF_KEY_LEFT:
            return 37;
        case TF_KEY_UP:
            return 38;
        case TF_KEY_RIGHT:
            return 39;
        case TF_KEY_DOWN:
            return 40;
        case TF_KEY_HOME:
            return 36;
        case TF_KEY_END:
            return 35;
        case TF_KEY_PGUP:
            return 33;
        case TF_KEY_PGDN:
            return 34;
        case TF_KEY_F:
            return 111 + (int32_t)key.ch; /* vbKeyF1 is 112 */
        default:
            return 0;
    }
}

/* The KeyPress character of a key (Enter 13, Backspace 8, Esc 27, Ctrl+A 1,
   a character its code point), or -1 for a key that has none. */
static int32_t key_ascii(tf_key key)
{
    switch (key.kind)
    {
        case TF_KEY_CHAR:
            return (int32_t)key.ch;
        case TF_KEY_ENTER:
            return 13;
        case TF_KEY_BACKSPACE:
            return 8;
        case TF_KEY_ESC:
            return 27;
        case TF_KEY_CTRL:
            return key.ch >= 'a' && key.ch <= 'z' ? (int32_t)(key.ch - 'a' + 1) : -1;
        default:
            return -1;
    }
}

/* One key for the form. Returns 1 if the form is to close. */
static int form_key(tf_control *form, tf_key key)
{
    if (menu_first(form, key))
    {
        return 0;
    }
    if (key.kind == TF_KEY_F && key.ch == 4 && (key.mods & (TF_MOD_CTRL | TF_MOD_ALT)) != 0)
    {
        return 1; /* Ctrl+F4 or Alt+F4 */
    }
    if (form->focus < 0 || !focusable(form->children[form->focus]))
    {
        move_focus(form, 1); /* the focused control was hidden or disabled */
    }
    /* KeyDown, then KeyPress, to the focused control first: either may
       discard the key (set it to 0), and KeyPress may change a character. */
    if (form->focus >= 0)
    {
        tf_control *c = form->children[form->focus];
        int32_t shift = 0;
        int32_t code = key_code(key, &shift);
        if (code != 0 && c->key_handler != NULL)
        {
            fire_key(c, &code, shift);
            if (code == 0)
            {
                return 0;
            }
        }
        int32_t ascii = key_ascii(key);
        if (ascii >= 0 && c->ref_handlers[TF_EVENT_KEYPRESS] != NULL)
        {
            const int32_t was = ascii;
            fire_ref(c, TF_EVENT_KEYPRESS, &ascii);
            if (ascii == 0)
            {
                return 0;
            }
            if (key.kind == TF_KEY_CHAR && ascii != was && ascii >= 32)
            {
                key.ch = (uint32_t)ascii;
            }
        }
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
    draw_stack();
    tf_screen_flush();
}

static int ui_is_open = 0;
static tf_control *last_closed = NULL; /* the last form shown, once none is */
static int ending = 0;                 /* the key script ran out: every loop ends */
static int dumped = 0;                 /* the headless screen was dumped when the script ended */

/* Registered as the shutdown hook: runs on normal close, End, or an error. */
static void ui_shutdown(void)
{
    if (headless)
    {
        /* Draw first: an error or End may come before the next redraw,
           and the dump should show what the program last set. */
        if (!dumped)
        {
            if (depth == 0 && last_closed != NULL)
            {
                stack[depth++] = last_closed; /* drawn as the user last saw it */
            }
            draw_stack();
            tf_screen_dump(stdout);
        }
    }
    else
    {
        tf_term_close();
    }
    tf_screen_free();
}

static void ui_open(void)
{
    if (ui_is_open)
    {
        return; /* a dialog in Form_Load or Sub Main opened it already */
    }
    ui_is_open = 1;
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

static int in_stack(const tf_control *form)
{
    for (int i = 0; i < depth; ++i)
    {
        if (stack[i] == form)
        {
            return 1;
        }
    }
    return 0;
}

static void remove_from_stack(const tf_control *form)
{
    for (int i = 0; i < depth; ++i)
    {
        if (stack[i] == form)
        {
            memmove(&stack[i], &stack[i + 1], (size_t)(depth - i - 1) * sizeof stack[0]);
            --depth;
            return;
        }
    }
}

/* Puts a form on top of the stack (moving it there if already shown), its
   Timers counting from now. (The focus is placed when it is activated.) */
static void push(tf_control *form)
{
    remove_from_stack(form);
    if (depth == TF_STACK_MAX)
    {
        tf_fatal_error(TF_ERR_OUT_OF_MEMORY, "too many forms shown at once");
    }
    stack[depth++] = form;
    for (int i = 0; i < form->child_count; ++i)
    {
        tf_control *c = form->children[i];
        c->due = now_ms() + c->interval;
    }
}

static tf_control *active = NULL; /* the form last at the top (for Activate) */

/* The event loop: keys go to the top form, while any form is shown, or
   until `until` is no longer shown (a modal form or a dialog). The key
   script running out ends every loop. Timers of all shown forms run. */
static void run_loop(const tf_control *until)
{
    while (!ending && depth > 0 && (until == NULL || in_stack(until)))
    {
        tf_control *top = stack[depth - 1];
        if (top != active)
        {
            /* Come to the top (shown, or uncovered): Activate, then the focus
               on its first control if it has none (GotFocus), as VB. */
            active = top;
            fire(top, TF_EVENT_ACTIVATE);
            if (in_stack(top) && top->focus < 0)
            {
                move_focus(top, 1);
            }
            continue; /* the handlers may have changed the stack */
        }
        draw_stack();
        tf_screen_flush();

        const int64_t due = stack_due();
        const int64_t wait = due < 0 ? -1 : due - now_ms();
        const tf_key key = tf_input_wait(due < 0 ? -1 : wait < 0 ? 0 : (int)wait);
        if (key.kind == TF_KEY_EOF)
        {
            /* Dump now: the screen as the script left it, a dialog and all. */
            draw_stack();
            tf_screen_dump(stdout);
            dumped = 1;
            ending = 1;
            break;
        }
        if (key.kind == TF_KEY_NONE)
        {
            stack_fire(); /* the wait ended: a Timer is due */
            continue;
        }
        if (key.kind == TF_KEY_WAIT)
        {
            wait_scripted(key.ch);
            continue;
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
        if (form_key(top, key))
        {
            /* Ctrl+F4: a form is unloaded (its Unload may refuse); a dialog
               is answered as by Esc, if it can be. */
            if (!top->is_dialog)
            {
                tf_form_unload(top);
            }
            else if (top->escapable)
            {
                tf_form_hide(top);
            }
        }
    }
}

void tf_form_load(tf_control *form)
{
    if (!form->loaded)
    {
        form->loaded = 1;
        fire(form, TF_EVENT_LOAD); /* before the form is first shown, as VB */
    }
}

void tf_form_show(tf_control *form, int32_t modal)
{
    tf_form_load(form);
    ui_open();
    push(form);
    if (modal)
    {
        run_loop(form);
    }
}

void tf_form_hide(tf_control *form)
{
    if (depth == 1 && stack[0] == form && !form->is_dialog)
    {
        last_closed = form; /* the screen the user last saw, for a headless dump */
    }
    remove_from_stack(form);
    if (active == form)
    {
        active = NULL;
    }
}

void tf_form_unload(tf_control *form)
{
    if (form->loaded)
    {
        int32_t cancel = 0;
        fire_ref(form, TF_EVENT_UNLOAD, &cancel);
        if (cancel != 0)
        {
            return; /* the program said no */
        }
    }
    form->loaded = 0;
    tf_form_hide(form);
}

int tf_forms_run(void)
{
    run_loop(NULL);
    tf_run_shutdown_hook();
    return 0;
}

int tf_form_run(tf_control *form)
{
    tf_form_show(form, 0);
    return tf_forms_run();
}

void tf_form_free(tf_control *form)
{
    for (int i = 0; i < form->child_count; ++i)
    {
        tf_control *c = form->children[i];
        tf_list_clear(c);
        free(c->items);
        free(c->selected);
        tf_str_release(c->caption);
        tf_str_release(c->text);
        free(c->name);
        free(c);
    }
    free(form->children);
    tf_str_release(form->caption);
    free(form->name);
    free(form);
}

/* ------------------------------------------------------------------------ */
/* MsgBox and InputBox                                                      */
/* ------------------------------------------------------------------------ */

/* The dialog being shown and the answer its buttons give (a Timer's handler
   may show another meanwhile, so these are saved around each). */
static tf_control *dialog = NULL;
static int32_t answer = 0;

#define ANSWER(name, value)                                                                        \
    static void name(void)                                                                         \
    {                                                                                              \
        answer = (value);                                                                          \
        tf_form_hide(dialog);                                                                      \
    }
ANSWER(on_ok, 1)
ANSWER(on_cancel, 2)
ANSWER(on_abort, 3)
ANSWER(on_retry, 4)
ANSWER(on_ignore, 5)
ANSWER(on_yes, 6)
ANSWER(on_no, 7)

static tf_string *text_of(const char *s)
{
    return tf_str_from_utf8(s, (int32_t)strlen(s));
}

static void set_caption(tf_control *c, const char *text)
{
    tf_string *s = text_of(text);
    tf_set_str(c, TF_PROP_CAPTION, s);
    tf_str_release(s);
}

/* The prompt's lines: split at line ends (CR LF, LF or CR) and wrapped at
   width, & doubled so a Label shows it as written. Returns the count. */
static int prompt_lines(const char *prompt, int width, char lines[][256], int most)
{
    int n = 0;
    const char *p = prompt;
    while (n < most)
    {
        const char *end = p;
        while (*end != '\0' && *end != '\r' && *end != '\n')
        {
            ++end;
        }
        /* Wrap this line at width code points, at a space if there is one. */
        const char *from = p;
        do
        {
            const char *cut = from;
            const char *space = NULL;
            int cells = 0;
            while (cut < end && cells < width)
            {
                if (*cut == ' ')
                {
                    space = cut;
                }
                uint32_t cp = 0;
                cut += tf_utf8_decode(cut, (int)(end - cut), &cp);
                ++cells;
            }
            if (cut < end && space != NULL && space > from)
            {
                cut = space;
            }
            size_t o = 0;
            for (const char *q = from; q < cut && o + 2 < sizeof lines[0]; ++q)
            {
                if (*q == '&')
                {
                    lines[n][o++] = '&';
                }
                lines[n][o++] = *q;
            }
            lines[n][o] = '\0';
            ++n;
            from = cut;
            while (from < end && *from == ' ')
            {
                ++from;
            }
        } while (from < end && n < most);
        if (*end == '\0')
        {
            break;
        }
        p = end + (end[0] == '\r' && end[1] == '\n' ? 2 : 1);
    }
    return n;
}

/* The program's name (its main file's, without folder or .bas), the title
   of a dialog that has none of its own, as VB uses the project's. */
static const char *program_name(void)
{
    static char name[128];
    const char *path = tf_runtime_source();
    const char *base = path;
    for (const char *p = path; *p != '\0'; ++p)
    {
        if (*p == '/' || *p == '\\')
        {
            base = p + 1;
        }
    }
    snprintf(name, sizeof name, "%s", base);
    char *dot = strrchr(name, '.');
    if (dot != NULL && dot != name)
    {
        *dot = '\0';
    }
    return name;
}

typedef struct button_spec
{
    const char *caption;
    tf_handler handler;
    int32_t value;
} button_spec;

/* Lays out a dialog: an optional icon, the prompt lines, an optional
   TextBox (InputBox), and a row of buttons; the form sized to fit. */
static tf_control *build_dialog(const char *title, const char *icon, const char *prompt,
                                const char *edit, const button_spec *buttons, int count,
                                int default_button, int cancel_button)
{
    int cols = 80;
    int rows = 25;
    tf_screen_size(&cols, &rows);
    const int most_width = cols - 8 < 64 ? (cols - 8 > 10 ? cols - 8 : 10) : 64;
    const int indent = icon != NULL ? 5 : 0;
    static char lines[16][256];
    const int n = prompt_lines(prompt, most_width - indent, lines, 16);

    int text_width = 0;
    for (int i = 0; i < n; ++i)
    {
        const int w = utf8_cells(lines[i]);
        text_width = w > text_width ? w : text_width;
    }
    int buttons_width = 0;
    for (int i = 0; i < count; ++i)
    {
        char plain[64];
        strip_accelerator(buttons[i].caption, plain, sizeof plain);
        buttons_width += utf8_cells(plain) + 4 + (i > 0 ? 2 : 0);
    }
    int inner = indent + text_width;
    inner = buttons_width > inner ? buttons_width : inner;
    inner = edit != NULL && inner < 30 ? 30 : inner;
    const int title_width = utf8_cells(title) + 4;
    inner = title_width > inner ? title_width : inner;

    tf_control *form = tf_form_new("Dialog");
    set_caption(form, title);
    const int edit_rows = edit != NULL ? 2 : 0;
    tf_set_int(form, TF_PROP_WIDTH, inner + 4);
    tf_set_int(form, TF_PROP_HEIGHT, n + edit_rows + 4);

    if (icon != NULL)
    {
        tf_control *mark = tf_control_new(form, TF_CTL_LABEL, "icon");
        set_caption(mark, icon);
        tf_set_int(mark, TF_PROP_LEFT, 1);
        tf_set_int(mark, TF_PROP_WIDTH, 3);
    }
    for (int i = 0; i < n; ++i)
    {
        tf_control *line = tf_control_new(form, TF_CTL_LABEL, "prompt");
        set_caption(line, lines[i]);
        tf_set_int(line, TF_PROP_LEFT, 1 + indent);
        tf_set_int(line, TF_PROP_TOP, i);
        tf_set_int(line, TF_PROP_WIDTH, inner - indent);
    }
    if (edit != NULL)
    {
        tf_control *box = tf_control_new(form, TF_CTL_TEXTBOX, "text");
        tf_string *s = text_of(edit);
        tf_set_str(box, TF_PROP_TEXT, s);
        tf_str_release(s);
        tf_set_int(box, TF_PROP_LEFT, 1);
        tf_set_int(box, TF_PROP_TOP, n);
        tf_set_int(box, TF_PROP_WIDTH, inner);
    }
    int x = 1 + (inner - buttons_width) / 2;
    for (int i = 0; i < count; ++i)
    {
        char plain[64];
        strip_accelerator(buttons[i].caption, plain, sizeof plain);
        tf_control *b = tf_control_new(form, TF_CTL_COMMANDBUTTON, "button");
        set_caption(b, buttons[i].caption);
        tf_set_int(b, TF_PROP_LEFT, x);
        tf_set_int(b, TF_PROP_TOP, n + edit_rows + 1);
        tf_set_int(b, TF_PROP_WIDTH, utf8_cells(plain) + 4);
        tf_control_on(b, TF_EVENT_CLICK, buttons[i].handler);
        b->is_default = i == default_button;
        b->is_cancel = i == cancel_button;
        x += utf8_cells(plain) + 6;
    }
    form->escapable = cancel_button >= 0;
    form->is_dialog = 1;
    form->loaded = 1;
    return form;
}

/* Shows a built dialog; the focus starts on its Default button (or, for an
   InputBox, in the TextBox), as VB. Returns the answer. */
static int32_t ask(tf_control *form, int32_t escape_answer, int focus_default)
{
    tf_control *saved_dialog = dialog;
    const int32_t saved_answer = answer;
    dialog = form;
    answer = escape_answer;
    ui_open();
    if (focus_default)
    {
        for (int i = 0; i < form->child_count; ++i)
        {
            if (form->children[i]->is_default)
            {
                form->focus = i;
            }
        }
    }
    push(form);
    run_loop(form);
    tf_form_hide(form); /* the key script may have ended with it shown */
    const int32_t result = answer;
    dialog = saved_dialog;
    answer = saved_answer;
    return result;
}

int32_t tf_msgbox(const tf_string *prompt, int32_t buttons, const tf_string *title)
{
    static const button_spec ok = {"OK", on_ok, 1}, cancel = {"Cancel", on_cancel, 2},
                             abort_ = {"&Abort", on_abort, 3}, retry = {"&Retry", on_retry, 4},
                             ignore = {"&Ignore", on_ignore, 5}, yes = {"&Yes", on_yes, 6},
                             no = {"&No", on_no, 7};
    button_spec set[3];
    int count = 0;
    int cancel_button = -1;
    switch (buttons & 7)
    {
        case 1: /* vbOKCancel */
            set[0] = ok, set[1] = cancel, count = 2, cancel_button = 1;
            break;
        case 2: /* vbAbortRetryIgnore */
            set[0] = abort_, set[1] = retry, set[2] = ignore, count = 3;
            break;
        case 3: /* vbYesNoCancel */
            set[0] = yes, set[1] = no, set[2] = cancel, count = 3, cancel_button = 2;
            break;
        case 4: /* vbYesNo */
            set[0] = yes, set[1] = no, count = 2;
            break;
        case 5: /* vbRetryCancel */
            set[0] = retry, set[1] = cancel, count = 2, cancel_button = 1;
            break;
        default: /* vbOKOnly: Esc is OK */
            set[0] = ok, count = 1, cancel_button = 0;
            break;
    }
    const int chosen = (buttons >> 8) & 3; /* vbDefaultButton2 = 256, 3 = 512 */
    const int default_button = chosen < count ? chosen : 0;
    const char *icon = NULL;
    switch (buttons & 0x70)
    {
        case 16:
            icon = "(X)"; /* vbCritical */
            break;
        case 32:
            icon = "(?)"; /* vbQuestion */
            break;
        case 48:
            icon = "(!)"; /* vbExclamation */
            break;
        case 64:
            icon = "(i)"; /* vbInformation */
            break;
        default:
            break;
    }
    const char *heading = tf_str_lenb(title) > 0 ? tf_str_cstr(title) : program_name();
    tf_control *form = build_dialog(heading, icon, tf_str_cstr(prompt), NULL, set, count,
                                    default_button, cancel_button);
    const int32_t result = ask(form, cancel_button >= 0 ? set[cancel_button].value : 0, 1);
    tf_form_free(form);
    return result;
}

tf_string *tf_inputbox(const tf_string *prompt, const tf_string *title, const tf_string *initial)
{
    static const button_spec ok = {"OK", on_ok, 1}, cancel = {"Cancel", on_cancel, 2};
    const button_spec set[2] = {ok, cancel};
    const char *heading = tf_str_lenb(title) > 0 ? tf_str_cstr(title) : program_name();
    tf_control *form =
        build_dialog(heading, NULL, tf_str_cstr(prompt), tf_str_cstr(initial), set, 2, 0, 1);
    const int32_t result = ask(form, 2, 0);
    /* OK gives the text; Cancel (or Esc) gives "", as VB. */
    tf_string *text = NULL;
    if (result == 1)
    {
        for (int i = 0; i < form->child_count; ++i)
        {
            if (form->children[i]->type == TF_CTL_TEXTBOX)
            {
                text = tf_str_ref(form->children[i]->text);
            }
        }
    }
    tf_form_free(form);
    return text;
}
