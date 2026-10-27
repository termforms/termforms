// SPDX-License-Identifier: BSD-2-Clause
//
// catalog.cpp -- see catalog.hpp.

#include "sema/catalog.hpp"

#include <cctype>

namespace tf::catalog
{

namespace
{

using ast::EventKind;
using ast::Type;

const std::vector<ControlType> &types()
{
    // Every control has a place and size, and Enabled and Visible.
    auto placed = [](std::vector<Property> own) {
        for (const Property &p : {Property{"Left", Type::Integer, "TF_PROP_LEFT"},
                                  Property{"Top", Type::Integer, "TF_PROP_TOP"},
                                  Property{"Width", Type::Integer, "TF_PROP_WIDTH"},
                                  Property{"Height", Type::Integer, "TF_PROP_HEIGHT"},
                                  Property{"Enabled", Type::Boolean, "TF_PROP_ENABLED"},
                                  Property{"Visible", Type::Boolean, "TF_PROP_VISIBLE"}})
        {
            own.push_back(p);
        }
        return own;
    };
    const Property caption{"Caption", Type::String, "TF_PROP_CAPTION"};
    const Event click{"Click", EventKind::Click, "TF_EVENT_CLICK"};
    // What every control that can take the focus raises besides its own.
    auto focusEvents = [](std::vector<Event> own) {
        for (const Event &e : {Event{"GotFocus", EventKind::GotFocus, "TF_EVENT_GOTFOCUS"},
                               Event{"LostFocus", EventKind::LostFocus, "TF_EVENT_LOSTFOCUS"},
                               Event{"KeyPress", EventKind::KeyPress, "TF_EVENT_KEYPRESS", "R",
                                     "(ByRef KeyAscii As Integer)"},
                               Event{"KeyDown", EventKind::KeyDown, "TF_EVENT_KEYDOWN", "RI",
                                     "(ByRef KeyCode As Integer, Shift As Integer)"}})
        {
            own.push_back(e);
        }
        return own;
    };
    static const std::vector<ControlType> kTypes = {
        {"Form",
         "TF_CTL_FORM",
         {caption,
          {"Width", Type::Integer, "TF_PROP_WIDTH"},
          {"Height", Type::Integer, "TF_PROP_HEIGHT"}},
         {{"Load", EventKind::Load, "TF_EVENT_LOAD"},
          {"Unload", EventKind::Unload, "TF_EVENT_UNLOAD", "R", "(ByRef Cancel As Integer)"},
          {"Activate", EventKind::Activate, "TF_EVENT_ACTIVATE"}}},
        {"TextBox", "TF_CTL_TEXTBOX", placed({{"Text", Type::String, "TF_PROP_TEXT"}}),
         focusEvents({{"Change", EventKind::Change, "TF_EVENT_CHANGE"}})},
        {"CommandButton", "TF_CTL_COMMANDBUTTON",
         placed({caption,
                 {"Default", Type::Boolean, "TF_PROP_DEFAULT"},
                 {"Cancel", Type::Boolean, "TF_PROP_CANCEL"}}),
         focusEvents({click})},
        {"Label", "TF_CTL_LABEL", placed({caption}), {click}},
        {"CheckBox", "TF_CTL_CHECKBOX",
         placed({caption, {"Value", Type::Integer, "TF_PROP_VALUE"}}), focusEvents({click})},
        {"OptionButton", "TF_CTL_OPTIONBUTTON",
         placed({caption, {"Value", Type::Boolean, "TF_PROP_VALUE"}}), focusEvents({click})},
        {"Frame", "TF_CTL_FRAME", placed({caption}), {}},
        // AddItem, RemoveItem, Clear, List(i) and Selected(i) are library
        // routines (ast.hpp's kStringFunctions), checked in sema.
        {"ListBox", "TF_CTL_LISTBOX",
         placed({{"Text", Type::String, "TF_PROP_TEXT"},
                 {"ListIndex", Type::Integer, "TF_PROP_LISTINDEX"},
                 {"ListCount", Type::Integer, "TF_PROP_LISTCOUNT", true},
                 {"MultiSelect", Type::Integer, "TF_PROP_MULTISELECT"},
                 {"Sorted", Type::Boolean, "TF_PROP_SORTED"},
                 {"SelCount", Type::Integer, "TF_PROP_SELCOUNT", true},
                 {"TopIndex", Type::Integer, "TF_PROP_TOPINDEX"}}),
         focusEvents({click, {"DblClick", EventKind::DblClick, "TF_EVENT_DBLCLICK"}})},
        // Never shown: it raises its Timer event every Interval ms.
        {"Timer",
         "TF_CTL_TIMER",
         {{"Interval", Type::Integer, "TF_PROP_INTERVAL"},
          {"Enabled", Type::Boolean, "TF_PROP_ENABLED"}},
         {{"Timer", EventKind::Timer, "TF_EVENT_TIMER"}}},
    };
    return kTypes;
}

} // namespace

bool equalsIgnoringCase(std::string_view a, std::string_view b)
{
    if (a.size() != b.size())
    {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i])))
        {
            return false;
        }
    }
    return true;
}

const ControlType *findType(std::string_view name)
{
    for (const ControlType &t : types())
    {
        if (name == t.name)
        {
            return &t;
        }
    }
    return nullptr;
}

const Property *findProperty(const ControlType &type, std::string_view name)
{
    for (const Property &p : type.properties)
    {
        if (name == p.name)
        {
            return &p;
        }
    }
    return nullptr;
}

const Event *findEvent(const ControlType &type, std::string_view name)
{
    for (const Event &e : type.events)
    {
        if (name == e.name)
        {
            return &e;
        }
    }
    return nullptr;
}

const Property *findPropertyIgnoringCase(const ControlType &type, std::string_view name)
{
    for (const Property &p : type.properties)
    {
        if (equalsIgnoringCase(name, p.name))
        {
            return &p;
        }
    }
    return nullptr;
}

const Event *findEventIgnoringCase(const ControlType &type, std::string_view name)
{
    for (const Event &e : type.events)
    {
        if (equalsIgnoringCase(name, e.name))
        {
            return &e;
        }
    }
    return nullptr;
}

} // namespace tf::catalog
