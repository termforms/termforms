# SPDX-License-Identifier: BSD-2-Clause
"""gdb support for TermForms programs: Strings shown as their text, arrays
and Dictionaries as their elements, Refs held in arrays as their records,
and stepping that passes over the runtime's own functions.

Load it in gdb (or in ~/.gdbinit):

    source /path/to/termforms/debug/gdb/termforms.py

In gdb a BASIC variable keeps its C name: x is v_x in a Sub, g_x at
module level, and a Type's members are f_<name>.

The layouts read here are the runtime's (tf_internal.h, tf_runtime.h,
tf_ref.c), as in debug/lldb/termforms.py; keep the three in step.
"""

import gdb

# tf_elem, as in tf_runtime.h.
ELEM_I32, ELEM_I64, ELEM_F64, ELEM_BOOL, ELEM_STRING, ELEM_RECORD, ELEM_REF, ELEM_U8 = range(8)
REF_HEADER_SIZE = 16  # a Ref's record follows its header (count, ops), rounded to 16
MOST_CHILDREN = 10000


def read_string(pointer):
    """A tf_string's text: its length (int32 after the refcount), then its
    UTF-8 bytes; NULL is the empty string."""
    address = int(pointer)
    if address == 0:
        return ""
    inferior = gdb.selected_inferior()
    length = int.from_bytes(inferior.read_memory(address + 4, 4).tobytes(), "little", signed=True)
    data = inferior.read_memory(address + 8, min(length, 4096)).tobytes() if length > 0 else b""
    return data.decode("utf-8", errors="replace") + ("..." if length > 4096 else "")


class StringPrinter:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            return read_string(self.val)
        except gdb.MemoryError:
            return "<unreadable String>"

    def display_hint(self):
        return "string"


def record_type(ops):
    """The C struct a tf_record_ops describes, by its name."""
    if int(ops) == 0:
        return None
    try:
        return gdb.lookup_type(ops["name"].string())
    except (gdb.error, gdb.MemoryError):
        return None


class ArrayPrinter:
    """A tf_array's elements: [i] or [i,j] (the first index changing
    fastest, as stored); a Dictionary's by key."""

    def __init__(self, val):
        self.val = val

    def count(self):
        return 0 if int(self.val["data"]) == 0 else int(self.val["count"])

    def to_string(self):
        n = self.count()
        if int(self.val["dict"]) != 0:
            return "Dictionary, %d %s" % (n, "key" if n == 1 else "keys")
        if n == 0:
            return "empty"
        rank = int(self.val["rank"])
        return "(" + ", ".join(str(int(self.val["upper"][i])) for i in range(rank)) + ")"

    def element_type(self):
        elem = int(self.val["elem"])
        names = {ELEM_I32: "int32_t", ELEM_I64: "int64_t", ELEM_F64: "double", ELEM_BOOL: "int",
                 ELEM_U8: "uint8_t"}
        if elem in names:
            return elem, gdb.lookup_type(names[elem])
        if elem == ELEM_STRING:
            return elem, gdb.lookup_type("tf_string").pointer()
        if elem == ELEM_REF:
            return elem, gdb.lookup_type("void").pointer()
        return elem, record_type(self.val["ops"])

    def children(self):
        elem, element = self.element_type()
        if element is None:
            return
        n = min(self.count(), MOST_CHILDREN)
        rank = int(self.val["rank"])
        extents = [int(self.val["upper"][i]) + 1 for i in range(rank)]
        base = int(self.val["data"])
        keys = None
        if int(self.val["dict"]) != 0:
            # tf_dict_index: keys first; key i goes with value i.
            index_type = gdb.lookup_type("tf_string").pointer().pointer().pointer()
            keys = gdb.Value(int(self.val["dict"])).cast(index_type).dereference()
        for index in range(n):
            value = gdb.Value(base + index * element.sizeof).cast(element.pointer()).dereference()
            if elem == ELEM_REF and int(value) != 0:
                # A Ref's record Type, from the ops in the header before it.
                ops_type = gdb.lookup_type("tf_record_ops").const().pointer()
                ops = gdb.Value(int(value) - REF_HEADER_SIZE + 8).cast(ops_type.pointer())
                t = record_type(ops.dereference())
                if t is not None:
                    value = value.cast(t.pointer())
            if keys is not None:
                name = '["' + read_string(keys[index]) + '"]'
            else:
                subscripts = []
                rest = index
                for extent in extents:
                    subscripts.append(rest % extent if extent else 0)
                    rest = rest // extent if extent else 0
                name = "[" + ",".join(str(s) for s in subscripts) + "]"
            yield name, value


def lookup(val):
    t = val.type.strip_typedefs()
    if t.code == gdb.TYPE_CODE_PTR and t.target().strip_typedefs().tag == "tf_string":
        return StringPrinter(val)
    if t.code == gdb.TYPE_CODE_STRUCT and t.tag == "tf_array":
        return ArrayPrinter(val)
    return None


gdb.pretty_printers.append(lookup)
# Step over the runtime and the Types' generated helpers, into BASIC.
gdb.execute("skip -rfunction ^(tf_|t_[A-Za-z0-9_]+_(init|free|copy|busy)$)", to_string=True)
