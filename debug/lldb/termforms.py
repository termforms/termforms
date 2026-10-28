# SPDX-License-Identifier: BSD-2-Clause
"""lldb support for TermForms programs: Strings shown as their text, arrays
and Dictionaries as their elements, Refs held in arrays as their records,
and stepping that passes over the runtime's own functions.

Load it in lldb (or in ~/.lldbinit, or a CodeLLDB launch.json's
initCommands):

    command script import /path/to/termforms/debug/lldb/termforms.py

In lldb a BASIC variable keeps its C name: x is v_x in a Sub, g_x at
module level, and a Type's members are f_<name>.

The layouts read here are the runtime's (tf_internal.h, tf_runtime.h,
tf_ref.c); keep them in step.
"""

import lldb

# tf_elem, as in tf_runtime.h.
ELEM_I32, ELEM_I64, ELEM_F64, ELEM_BOOL, ELEM_STRING, ELEM_RECORD, ELEM_REF, ELEM_U8 = range(8)
# A Ref's record follows its header (count, ops) rounded to 16 bytes (tf_ref.c).
REF_HEADER_SIZE = 16
MOST_CHILDREN = 10000


def read_string(process, address):
    """A tf_string's text: its length (int32 after the refcount), then its
    UTF-8 bytes; NULL is the empty string."""
    if address == 0:
        return ""
    error = lldb.SBError()
    length = process.ReadUnsignedFromMemory(address + 4, 4, error)
    if not error.Success():
        return None
    data = process.ReadMemory(address + 8, min(length, 4096), error) if length else b""
    if not error.Success():
        return None
    text = data.decode("utf-8", errors="replace")
    return text + ("..." if length > 4096 else "")


def string_summary(valobj, internal_dict):
    text = read_string(valobj.GetProcess(), valobj.GetValueAsUnsigned(0))
    if text is None:
        return "<unreadable String>"
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


def element_type(target, valobj):
    """The C type of an array's elements, and their size; a Record array's
    Type comes from its ops' name."""
    elem = valobj.GetChildMemberWithName("elem").GetValueAsUnsigned(0)
    names = {ELEM_I32: "int32_t", ELEM_I64: "int64_t", ELEM_F64: "double", ELEM_BOOL: "int",
             ELEM_U8: "uint8_t"}
    if elem in names:
        t = target.FindFirstType(names[elem])
        if not t.IsValid():
            basic = {ELEM_I32: lldb.eBasicTypeInt, ELEM_I64: lldb.eBasicTypeLongLong,
                     ELEM_F64: lldb.eBasicTypeDouble, ELEM_BOOL: lldb.eBasicTypeInt,
                     ELEM_U8: lldb.eBasicTypeUnsignedChar}[elem]
            t = target.GetBasicType(basic)
        return elem, t, t.GetByteSize()
    if elem in (ELEM_STRING, ELEM_REF):
        t = target.FindFirstType("tf_string").GetPointerType() if elem == ELEM_STRING \
            else target.GetBasicType(lldb.eBasicTypeVoid).GetPointerType()
        return elem, t, target.GetAddressByteSize()
    ops = valobj.GetChildMemberWithName("ops")
    name = ops.Dereference().GetChildMemberWithName("name").GetSummary()
    t = target.FindFirstType(name.strip('"')) if name else lldb.SBType()
    return elem, t, ops.Dereference().GetChildMemberWithName("size").GetValueAsUnsigned(0)


def record_type_of_ref(target, process, address):
    """A Ref's record Type, from the ops in the header before it."""
    if address == 0:
        return None
    error = lldb.SBError()
    ops = process.ReadPointerFromMemory(address - REF_HEADER_SIZE + 8, error)
    if not error.Success() or ops == 0:
        return None
    # tf_record_ops: size, init, free, copy, then name.
    name_address = process.ReadPointerFromMemory(ops + 4 * target.GetAddressByteSize(), error)
    if not error.Success():
        return None
    name = process.ReadCStringFromMemory(name_address, 128, error)
    t = target.FindFirstType(name) if error.Success() else lldb.SBType()
    return t if t.IsValid() else None


class ArrayProvider:
    """A tf_array's elements as its children: [i] or [i,j] (the first index
    changing fastest, as stored); a Dictionary's by key."""

    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.update()

    def update(self):
        v = self.valobj
        self.target = v.GetTarget()
        self.process = v.GetProcess()
        self.data = v.GetChildMemberWithName("data").GetValueAsUnsigned(0)
        self.count = v.GetChildMemberWithName("count").GetValueAsUnsigned(0)
        self.rank = v.GetChildMemberWithName("rank").GetValueAsUnsigned(0)
        upper = v.GetChildMemberWithName("upper")
        self.extents = [upper.GetChildAtIndex(i).GetValueAsSigned(0) + 1 for i in range(self.rank)]
        self.dict = v.GetChildMemberWithName("dict").GetValueAsUnsigned(0)
        self.elem, self.type, self.size = element_type(self.target, v)
        if self.data == 0:
            self.count = 0
        return False

    def num_children(self):
        return min(self.count, MOST_CHILDREN)

    def name_of(self, index):
        if self.dict:
            # tf_dict_index: keys first; key i goes with value i.
            error = lldb.SBError()
            keys = self.process.ReadPointerFromMemory(self.dict, error)
            key = self.process.ReadPointerFromMemory(
                keys + index * self.target.GetAddressByteSize(), error)
            return '["' + (read_string(self.process, key) or "?") + '"]'
        subscripts = []
        rest = index
        for extent in self.extents:
            subscripts.append(rest % extent if extent else 0)
            rest = rest // extent if extent else 0
        return "[" + ",".join(str(s) for s in subscripts) + "]"

    def get_child_at_index(self, index):
        if index >= self.num_children() or not self.type.IsValid():
            return None
        address = self.data + index * self.size
        name = self.name_of(index)
        if self.elem == ELEM_REF:
            error = lldb.SBError()
            record = self.process.ReadPointerFromMemory(address, error)
            t = record_type_of_ref(self.target, self.process, record)
            if t is not None:
                return self.valobj.CreateValueFromAddress(name, address, t.GetPointerType())
        return self.valobj.CreateValueFromAddress(name, address, self.type)

    def get_child_index(self, name):
        return -1

    def has_children(self):
        return True


def array_summary(valobj, internal_dict):
    raw = valobj.GetNonSyntheticValue()
    count = raw.GetChildMemberWithName("count").GetValueAsUnsigned(0)
    if raw.GetChildMemberWithName("data").GetValueAsUnsigned(0) == 0:
        count = 0
    if raw.GetChildMemberWithName("dict").GetValueAsUnsigned(0):
        return "Dictionary, %d %s" % (count, "key" if count == 1 else "keys")
    if count == 0:
        return "empty"
    rank = raw.GetChildMemberWithName("rank").GetValueAsUnsigned(0)
    upper = raw.GetChildMemberWithName("upper")
    bounds = ", ".join(str(upper.GetChildAtIndex(i).GetValueAsSigned(0)) for i in range(rank))
    return "(" + bounds + ")"


def __lldb_init_module(debugger, internal_dict):
    module = __name__
    debugger.HandleCommand(
        'type summary add -F %s.string_summary "tf_string *" -w termforms' % module)
    debugger.HandleCommand(
        'type synthetic add -l %s.ArrayProvider tf_array -w termforms' % module)
    debugger.HandleCommand(
        'type summary add -e -F %s.array_summary tf_array -w termforms' % module)
    debugger.HandleCommand("type category enable termforms")
    # Step over the runtime and the Types' generated helpers, into BASIC.
    # (Unquoted: lldb would keep quotes as part of the expression.)
    debugger.HandleCommand(
        'settings set target.process.thread.step-avoid-regexp '
        '^(tf_|t_[A-Za-z0-9_]+_(init|free|copy|busy)$)')
