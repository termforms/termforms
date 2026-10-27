// SPDX-License-Identifier: BSD-2-Clause
//
// catalog.hpp -- what the runtime provides: control types, their properties
// and their events. The semantic pass checks programs against this table and
// the code generator takes the runtime's C names from it, so adding a control
// or property starts here (and in the runtime's tf_runtime.h).

#pragma once

#include "ast/ast.hpp"

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
};

struct Event
{
    const char *name; // e.g. "Click"
    ast::EventKind kind;
    const char *cName; // runtime enum, or nullptr if called directly (Load)
};

struct ControlType
{
    const char *name;  // e.g. "TextBox"
    const char *cName; // runtime enum, e.g. "TF_CTL_TEXTBOX"
    std::vector<Property> properties;
    std::vector<Event> events;
};

// Looks up a control type by its exact (case-sensitive) name; "Form" included.
const ControlType *findType(std::string_view name);

const Property *findProperty(const ControlType &type, std::string_view name);
const Event *findEvent(const ControlType &type, std::string_view name);

// Case-insensitive variants, used only to suggest corrections.
const Property *findPropertyIgnoringCase(const ControlType &type, std::string_view name);
const Event *findEventIgnoringCase(const ControlType &type, std::string_view name);

bool equalsIgnoringCase(std::string_view a, std::string_view b);

} // namespace tf::catalog
