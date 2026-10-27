/* SPDX-License-Identifier: 0BSD */
/*
 * tf_edit.c -- the edit buffer under a TextBox: text, caret, selection,
 * the editing keys, undo and the clipboard.
 *
 * Kept apart from the controls so the multi-line TextBox and the ComboBox
 * (after v1) edit the same way. Offsets are bytes into the UTF-8 text and
 * always sit on a code point's first byte; the program sees code points
 * (SelStart, SelLength), as Len and Mid$ count them.
 *
 * Keys follow a Windows edit control, as VB's TextBox: Shift extends the
 * selection, Ctrl moves by words, typing replaces the selection, Ctrl+Z
 * undoes the last change (and again redoes it; a run of typing is one
 * change), Ctrl+A selects all, Ctrl+X/C/V cut, copy and paste.
 *
 * The clipboard is the program's own: a terminal program cannot reach the
 * system's clipboard portably (the terminal's own copy and paste still
 * work, by Shift+drag and its paste key).
 */
#include "tf_tui.h"

#include <stdlib.h>
#include <string.h>

static tf_string *clipboard = NULL;

static int32_t length_of(const tf_edit *e)
{
    return tf_str_lenb(e->text);
}

static int is_lead(unsigned char c)
{
    return (c & 0xC0u) != 0x80u;
}

static int32_t count_code_points(const char *bytes, int32_t from, int32_t to)
{
    int32_t n = 0;
    for (int32_t i = from; i < to; ++i)
    {
        n += is_lead((unsigned char)bytes[i]);
    }
    return n;
}

/* The byte offset of code point index cp (clamped to the end). */
static int32_t offset_of(const tf_edit *e, int32_t cp)
{
    const char *bytes = tf_str_cstr(e->text);
    const int32_t length = length_of(e);
    int32_t i = 0;
    for (; i < length; ++i)
    {
        if (is_lead((unsigned char)bytes[i]))
        {
            if (cp == 0)
            {
                return i;
            }
            --cp;
        }
    }
    return length;
}

static int32_t step_back(const tf_edit *e, int32_t at)
{
    const char *bytes = tf_str_cstr(e->text);
    while (at > 0)
    {
        --at;
        if (is_lead((unsigned char)bytes[at]))
        {
            break;
        }
    }
    return at;
}

static int32_t step_on(const tf_edit *e, int32_t at)
{
    const char *bytes = tf_str_cstr(e->text);
    const int32_t length = length_of(e);
    if (at < length)
    {
        ++at;
        while (at < length && !is_lead((unsigned char)bytes[at]))
        {
            ++at;
        }
    }
    return at;
}

/* Part of a word: a letter, digit or underscore; anything beyond ASCII
   counts too (a word of another script should move as one). */
static int word_byte(unsigned char c)
{
    return c >= 0x80 || c == '_' || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z');
}

/* Ctrl+Right: to the start of the next word (as Windows). */
static int32_t next_word(const tf_edit *e, int32_t at)
{
    const char *bytes = tf_str_cstr(e->text);
    const int32_t length = length_of(e);
    while (at < length && word_byte((unsigned char)bytes[at]))
    {
        ++at;
    }
    while (at < length && !word_byte((unsigned char)bytes[at]))
    {
        ++at;
    }
    return at;
}

/* Ctrl+Left: to the start of this word, or of the one before. */
static int32_t previous_word(const tf_edit *e, int32_t at)
{
    const char *bytes = tf_str_cstr(e->text);
    while (at > 0 && !word_byte((unsigned char)bytes[at - 1]))
    {
        --at;
    }
    while (at > 0 && word_byte((unsigned char)bytes[at - 1]))
    {
        --at;
    }
    return at;
}

static int32_t sel_from(const tf_edit *e)
{
    return e->cursor < e->anchor ? e->cursor : e->anchor;
}

static int32_t sel_to(const tf_edit *e)
{
    return e->cursor < e->anchor ? e->anchor : e->cursor;
}

static void remember(tf_edit *e)
{
    tf_str_assign(&e->undo, tf_str_ref(e->text));
    e->undo_cursor = e->cursor;
    e->has_undo = 1;
}

/* Replaces bytes [from, to) with insert; the caret goes after it. */
static void splice(tf_edit *e, int32_t from, int32_t to, const char *insert, int32_t length)
{
    const char *old = tf_str_cstr(e->text);
    const int32_t old_length = length_of(e);
    const int32_t new_length = old_length - (to - from) + length;

    char *buffer = tf_xmalloc((size_t)new_length + 1);
    memcpy(buffer, old, (size_t)from);
    memcpy(buffer + from, insert, (size_t)length);
    memcpy(buffer + from + length, old + to, (size_t)(old_length - to));
    tf_str_assign(&e->text, tf_str_from_utf8(buffer, new_length));
    free(buffer);
    e->cursor = from + length;
    e->anchor = e->cursor;
}

/* How many bytes of insert fit under MaxLength once the selection is gone,
   cut back to whole code points. */
static int32_t fitting(const tf_edit *e, const char *insert, int32_t length)
{
    if (e->max_length <= 0)
    {
        return length;
    }
    const char *bytes = tf_str_cstr(e->text);
    const int32_t kept = count_code_points(bytes, 0, length_of(e)) -
                         count_code_points(bytes, sel_from(e), sel_to(e));
    int32_t room = e->max_length - kept;
    int32_t i = 0;
    for (; i < length; ++i)
    {
        if (is_lead((unsigned char)insert[i]))
        {
            if (room == 0)
            {
                break;
            }
            --room;
        }
    }
    return i;
}

/* Puts text where the selection is (the user typing or pasting): limited by
   MaxLength. Returns 1 if the text changed. */
static int type_in(tf_edit *e, const char *insert, int32_t length, int typing)
{
    const int32_t n = fitting(e, insert, length);
    if (n == 0 && sel_from(e) == sel_to(e))
    {
        return 0;
    }
    if (!(typing && e->typing))
    {
        remember(e); /* a run of typing is undone as one */
    }
    splice(e, sel_from(e), sel_to(e), insert, n);
    e->typing = typing;
    return 1;
}

static int cut_selection(tf_edit *e)
{
    if (sel_from(e) == sel_to(e))
    {
        return 0;
    }
    remember(e);
    splice(e, sel_from(e), sel_to(e), "", 0);
    e->typing = 0;
    return 1;
}

static void copy_selection(const tf_edit *e)
{
    if (sel_from(e) == sel_to(e) || e->concealed)
    {
        return; /* nothing to copy, or a password, which is not given out */
    }
    tf_str_assign(&clipboard,
                  tf_str_from_utf8(tf_str_cstr(e->text) + sel_from(e), sel_to(e) - sel_from(e)));
}

static int undo(tf_edit *e)
{
    if (!e->has_undo)
    {
        return 0;
    }
    /* Swap: the next Ctrl+Z puts the change back, as Windows. */
    tf_string *was = tf_str_ref(e->text);
    const int32_t was_cursor = e->cursor;
    tf_str_assign(&e->text, tf_str_ref(e->undo));
    e->cursor = e->undo_cursor <= length_of(e) ? e->undo_cursor : length_of(e);
    e->anchor = e->cursor;
    tf_str_assign(&e->undo, was);
    e->undo_cursor = was_cursor;
    e->typing = 0;
    return 1;
}

/* Moves the caret to `to`; with Shift the selection grows from the anchor,
   else it goes. */
static int move(tf_edit *e, int32_t to, int extend)
{
    e->cursor = to;
    if (!extend)
    {
        e->anchor = to;
    }
    e->typing = 0;
    return TF_EDIT_MOVED;
}

int tf_edit_key(tf_edit *e, tf_key key)
{
    const int shift = (key.mods & TF_MOD_SHIFT) != 0;
    const int ctrl = (key.mods & TF_MOD_CTRL) != 0;
    const int selected = sel_from(e) != sel_to(e);
    switch (key.kind)
    {
        case TF_KEY_CHAR:
        {
            if (e->locked)
            {
                return TF_EDIT_MOVED; /* used up: a Locked box takes no typing */
            }
            char encoded[4];
            const int n = tf_utf8_encode(key.ch, encoded);
            return type_in(e, encoded, n, 1) ? TF_EDIT_CHANGED : TF_EDIT_MOVED;
        }
        case TF_KEY_BACKSPACE:
        case TF_KEY_DELETE:
            if (e->locked)
            {
                return TF_EDIT_MOVED;
            }
            if (!selected)
            {
                /* Select the code point to go, then cut it. */
                if (key.kind == TF_KEY_BACKSPACE ? e->cursor == 0 : e->cursor == length_of(e))
                {
                    return TF_EDIT_MOVED;
                }
                e->anchor =
                    key.kind == TF_KEY_BACKSPACE ? step_back(e, e->cursor) : step_on(e, e->cursor);
            }
            return cut_selection(e) ? TF_EDIT_CHANGED : TF_EDIT_MOVED;
        case TF_KEY_LEFT:
            if (selected && !shift && !ctrl)
            {
                return move(e, sel_from(e), 0); /* to the selection's start */
            }
            return move(e, ctrl ? previous_word(e, e->cursor) : step_back(e, e->cursor), shift);
        case TF_KEY_RIGHT:
            if (selected && !shift && !ctrl)
            {
                return move(e, sel_to(e), 0);
            }
            return move(e, ctrl ? next_word(e, e->cursor) : step_on(e, e->cursor), shift);
        case TF_KEY_HOME:
            return move(e, 0, shift);
        case TF_KEY_END:
            return move(e, length_of(e), shift);
        case TF_KEY_CTRL:
            switch (key.ch)
            {
                case 'a':
                    e->anchor = 0;
                    return move(e, length_of(e), 1);
                case 'c':
                    copy_selection(e);
                    return TF_EDIT_MOVED;
                case 'x':
                    if (e->locked || e->concealed)
                    {
                        return TF_EDIT_MOVED;
                    }
                    copy_selection(e);
                    return cut_selection(e) ? TF_EDIT_CHANGED : TF_EDIT_MOVED;
                case 'v':
                    if (e->locked || clipboard == NULL)
                    {
                        return TF_EDIT_MOVED;
                    }
                    return type_in(e, tf_str_cstr(clipboard), tf_str_lenb(clipboard), 0)
                               ? TF_EDIT_CHANGED
                               : TF_EDIT_MOVED;
                case 'z':
                    return !e->locked && undo(e) ? TF_EDIT_CHANGED : TF_EDIT_MOVED;
                default:
                    return TF_EDIT_UNUSED;
            }
        default:
            return TF_EDIT_UNUSED;
    }
}

void tf_edit_set_text(tf_edit *e, const tf_string *text)
{
    tf_string *v = (tf_string *)text;
    tf_str_retain(v);
    tf_str_assign(&e->text, v);
    e->cursor = length_of(e); /* the caret to the end, as in VB */
    e->anchor = e->cursor;
    e->has_undo = 0; /* the program's change is not the user's to undo */
    e->typing = 0;
}

int32_t tf_edit_sel_start(const tf_edit *e)
{
    return count_code_points(tf_str_cstr(e->text), 0, sel_from(e));
}

int32_t tf_edit_sel_length(const tf_edit *e)
{
    return count_code_points(tf_str_cstr(e->text), sel_from(e), sel_to(e));
}

tf_string *tf_edit_sel_text(const tf_edit *e)
{
    return tf_str_from_utf8(tf_str_cstr(e->text) + sel_from(e), sel_to(e) - sel_from(e));
}

void tf_edit_select(tf_edit *e, int32_t start, int32_t length)
{
    e->anchor = offset_of(e, start);
    e->cursor = offset_of(e, start + length); /* the caret at the end, as VB */
    e->typing = 0;
}

void tf_edit_place(tf_edit *e, int32_t cp, int extend)
{
    move(e, offset_of(e, cp), extend);
}

void tf_edit_select_word(tf_edit *e, int32_t cp)
{
    const char *bytes = tf_str_cstr(e->text);
    const int32_t length = length_of(e);
    int32_t from = offset_of(e, cp);
    int32_t to = from;
    if (from < length && word_byte((unsigned char)bytes[from]))
    {
        while (from > 0 && word_byte((unsigned char)bytes[from - 1]))
        {
            --from;
        }
        while (to < length && word_byte((unsigned char)bytes[to]))
        {
            ++to;
        }
    }
    e->anchor = from;
    e->cursor = to;
    e->typing = 0;
}

void tf_edit_replace_selection(tf_edit *e, const tf_string *text)
{
    remember(e);
    splice(e, sel_from(e), sel_to(e), tf_str_cstr(text), tf_str_lenb(text));
    e->typing = 0;
}

int32_t tf_edit_cursor_index(const tf_edit *e)
{
    return count_code_points(tf_str_cstr(e->text), 0, e->cursor);
}

int tf_edit_selected(const tf_edit *e, int32_t cp)
{
    const char *bytes = tf_str_cstr(e->text);
    const int32_t from = count_code_points(bytes, 0, sel_from(e));
    const int32_t to = from + count_code_points(bytes, sel_from(e), sel_to(e));
    return cp >= from && cp < to;
}

void tf_edit_free(tf_edit *e)
{
    tf_str_release(e->text);
    tf_str_release(e->undo);
    e->text = NULL;
    e->undo = NULL;
}
