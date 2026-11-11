// SPDX-License-Identifier: BSD-2-Clause
//
// catalog.hpp -- what the runtime provides: control types, their properties
// and their events. The semantic pass checks programs against this table and
// the code generator takes the runtime's C names from it, so adding a control
// or property starts here (and in the runtime's tf_runtime.h).

#pragma once

#include "ast/ast.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace tf::catalog
{

struct Property
{
    const char *name; // as written in BASIC, e.g. "Caption"
    ast::Type type;
    const char *cName;     // runtime enum, e.g. "TF_PROP_CAPTION"
    bool readOnly = false; // ListCount: read, never set (in the form block or in code)
    bool formOnly = false; // MultiLine: set in the form block only, read anywhere
};

struct Event
{
    const char *name; // e.g. "Click"
    ast::EventKind kind;
    const char *cName; // runtime enum
    // The handler's parameters: R a ByRef Integer the handler may change, Q
    // a ByRef Boolean likewise (Validate's Cancel, as VB6), I a ByVal
    // Integer; "" for none. signature shows them in messages.
    const char *params = "";
    const char *signature = "";
};

struct ControlType
{
    const char *name;  // e.g. "TextBox"
    const char *cName; // runtime enum, e.g. "TF_CTL_TEXTBOX"
    std::vector<Property> properties;
    std::vector<Event> events;
};

// Every control type, "Form" first (for tfc --catalog and the FormView's table).
const std::vector<ControlType> &allTypes();

// tfc --catalog: every control, then its properties and events, one a line,
// for a tool (a form designer) to read without parsing JSON:
//   control CommandButton
//   property Caption String
//   property ListCount Integer read-only      (or form-only: the form block's)
//   event KeyDown ByRef KeyCode As Integer, Shift As Integer
std::string text();

// Looks up a control type by its exact (case-sensitive) name; "Form" included.
const ControlType *findType(std::string_view name);

const Property *findProperty(const ControlType &type, std::string_view name);
const Event *findEvent(const ControlType &type, std::string_view name);

// Case-insensitive variants, used only to suggest corrections.
const Property *findPropertyIgnoringCase(const ControlType &type, std::string_view name);
const Event *findEventIgnoringCase(const ControlType &type, std::string_view name);

bool equalsIgnoringCase(std::string_view a, std::string_view b);

} // namespace tf::catalog
