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
    // ...and a control that can take the focus also has CausesValidation:
    // whether moving to it first runs the Validate of the one being left.
    auto focusable = [&placed](std::vector<Property> own) {
        own.push_back({"CausesValidation", Type::Boolean, "TF_PROP_CAUSESVALIDATION"});
        return placed(own);
    };
    const Property caption{"Caption", Type::String, "TF_PROP_CAPTION"};
    const Event click{"Click", EventKind::Click, "TF_EVENT_CLICK"};
    // What every control the mouse can press raises besides its own (as
    // VB): MouseMove only while a button is held.
    auto mouse = [](std::vector<Event> own) {
        static const char *const kSignature =
            "(Button As Integer, Shift As Integer, X As Integer, Y As Integer)";
        for (const Event &e :
             {Event{"MouseDown", EventKind::MouseDown, "TF_EVENT_MOUSEDOWN", "IIII", kSignature},
              Event{"MouseMove", EventKind::MouseMove, "TF_EVENT_MOUSEMOVE", "IIII", kSignature},
              Event{"MouseUp", EventKind::MouseUp, "TF_EVENT_MOUSEUP", "IIII", kSignature}})
        {
            own.push_back(e);
        }
        return own;
    };
    // What every control that can take the focus raises besides its own.
    // Validate (as VB6) comes before the focus leaves for a control whose
    // CausesValidation is True; Cancel keeps it.
    auto focusEvents = [&mouse](std::vector<Event> own) {
        for (const Event &e : {Event{"GotFocus", EventKind::GotFocus, "TF_EVENT_GOTFOCUS"},
                               Event{"LostFocus", EventKind::LostFocus, "TF_EVENT_LOSTFOCUS"},
                               Event{"Validate", EventKind::Validate, "TF_EVENT_VALIDATE", "Q",
                                     "(ByRef Cancel As Boolean)"},
                               Event{"KeyPress", EventKind::KeyPress, "TF_EVENT_KEYPRESS", "R",
                                     "(ByRef KeyAscii As Integer)"},
                               Event{"KeyDown", EventKind::KeyDown, "TF_EVENT_KEYDOWN", "RI",
                                     "(ByRef KeyCode As Integer, Shift As Integer)"}})
        {
            own.push_back(e);
        }
        return mouse(own);
    };
    static const std::vector<ControlType> kTypes = {
        {"Form",
         "TF_CTL_FORM",
         {caption,
          {"Width", Type::Integer, "TF_PROP_WIDTH"},
          {"Height", Type::Integer, "TF_PROP_HEIGHT"},
          // Whether it is shown (Show and Hide change it), e.g. after a
          // DoEvents in which the user may have closed it.
          {"Visible", Type::Boolean, "TF_PROP_VISIBLE", true}},
         mouse({{"Load", EventKind::Load, "TF_EVENT_LOAD"},
                {"Unload", EventKind::Unload, "TF_EVENT_UNLOAD", "R", "(ByRef Cancel As Integer)"},
                {"Activate", EventKind::Activate, "TF_EVENT_ACTIVATE"},
                // When shown, and whenever the terminal's size changes.
                {"Resize", EventKind::Resize, "TF_EVENT_RESIZE"}})},
        {"TextBox", "TF_CTL_TEXTBOX",
         focusable({{"Text", Type::String, "TF_PROP_TEXT"},
                    {"SelStart", Type::Integer, "TF_PROP_SELSTART"},
                    {"SelLength", Type::Integer, "TF_PROP_SELLENGTH"},
                    {"SelText", Type::String, "TF_PROP_SELTEXT"},
                    {"MaxLength", Type::Integer, "TF_PROP_MAXLENGTH"},
                    {"Locked", Type::Boolean, "TF_PROP_LOCKED"},
                    {"PasswordChar", Type::String, "TF_PROP_PASSWORDCHAR"},
                    // Fixed at design time, as VB: they shape how the box lays out.
                    {"MultiLine", Type::Boolean, "TF_PROP_MULTILINE", false, true},
                    {"ScrollBars", Type::Integer, "TF_PROP_SCROLLBARS", false, true}}),
         focusEvents({{"Change", EventKind::Change, "TF_EVENT_CHANGE"}})},
        {"CommandButton", "TF_CTL_COMMANDBUTTON",
         focusable({caption,
                    {"Default", Type::Boolean, "TF_PROP_DEFAULT"},
                    {"Cancel", Type::Boolean, "TF_PROP_CANCEL"}}),
         focusEvents({click})},
        {"Label", "TF_CTL_LABEL", placed({caption}), mouse({click})},
        {"CheckBox", "TF_CTL_CHECKBOX",
         focusable({caption, {"Value", Type::Integer, "TF_PROP_VALUE"}}), focusEvents({click})},
        {"OptionButton", "TF_CTL_OPTIONBUTTON",
         focusable({caption, {"Value", Type::Boolean, "TF_PROP_VALUE"}}), focusEvents({click})},
        {"Frame", "TF_CTL_FRAME", placed({caption}), mouse({})},
        // AddItem, RemoveItem, Clear, List(i) and Selected(i) are library
        // routines (ast.hpp's kStringFunctions), checked in sema.
        {"ListBox", "TF_CTL_LISTBOX",
         focusable({{"Text", Type::String, "TF_PROP_TEXT"},
                    {"ListIndex", Type::Integer, "TF_PROP_LISTINDEX"},
                    {"ListCount", Type::Integer, "TF_PROP_LISTCOUNT", true},
                    {"MultiSelect", Type::Integer, "TF_PROP_MULTISELECT"},
                    {"Sorted", Type::Boolean, "TF_PROP_SORTED"},
                    {"SelCount", Type::Integer, "TF_PROP_SELCOUNT", true},
                    {"TopIndex", Type::Integer, "TF_PROP_TOPINDEX"}}),
         focusEvents({click, {"DblClick", EventKind::DblClick, "TF_EVENT_DBLCLICK"}})},
        // A ListBox's items (AddItem, RemoveItem, Clear, List(i) as there),
        // one shown, with a list that drops down: Style 0 a text box too,
        // Style 2 the list only.
        {"ComboBox", "TF_CTL_COMBOBOX",
         focusable({{"Text", Type::String, "TF_PROP_TEXT"},
                    {"ListIndex", Type::Integer, "TF_PROP_LISTINDEX"},
                    {"ListCount", Type::Integer, "TF_PROP_LISTCOUNT", true},
                    {"Sorted", Type::Boolean, "TF_PROP_SORTED"},
                    {"Style", Type::Integer, "TF_PROP_STYLE", false, true},
                    {"SelStart", Type::Integer, "TF_PROP_SELSTART"},
                    {"SelLength", Type::Integer, "TF_PROP_SELLENGTH"},
                    {"SelText", Type::String, "TF_PROP_SELTEXT"},
                    {"MaxLength", Type::Integer, "TF_PROP_MAXLENGTH"},
                    {"Locked", Type::Boolean, "TF_PROP_LOCKED"}}),
         focusEvents({click,
                      {"Change", EventKind::Change, "TF_EVENT_CHANGE"},
                      {"DropDown", EventKind::DropDown, "TF_EVENT_DROPDOWN"}})},
        // Shows a form described in code (AddControl, SetProperty...), as
        // it would run but not live: for a visual designer. Its methods
        // are library routines (ast.hpp's kStringFunctions), checked in sema.
        {"FormView", "TF_CTL_FORMVIEW",
         focusable({{"ScrollX", Type::Integer, "TF_PROP_SCROLLX"},
                    {"ScrollY", Type::Integer, "TF_PROP_SCROLLY"},
                    {"TabNumbers", Type::Boolean, "TF_PROP_TABNUMBERS"}}),
         focusEvents({click, {"DblClick", EventKind::DblClick, "TF_EVENT_DBLCLICK"}})},
        // Never shown: it raises its Timer event every Interval ms.
        {"Timer",
         "TF_CTL_TIMER",
         {{"Interval", Type::Integer, "TF_PROP_INTERVAL"},
          {"Enabled", Type::Boolean, "TF_PROP_ENABLED"}},
         {{"Timer", EventKind::Timer, "TF_EVENT_TIMER"}}},
        // On the form's menu bar, or in a menu's drop-down (sema keeps them
        // to those two levels, as VB-DOS). A Caption of "-" is a separator.
        {"Menu",
         "TF_CTL_MENU",
         {caption,
          {"Enabled", Type::Boolean, "TF_PROP_ENABLED"},
          {"Visible", Type::Boolean, "TF_PROP_VISIBLE"},
          {"Checked", Type::Boolean, "TF_PROP_CHECKED"},
          {"Shortcut", Type::String, "TF_PROP_SHORTCUT"}},
         {click}},
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

const std::vector<ControlType> &allTypes()
{
    return types();
}

std::string text()
{
    std::string out;
    for (const ControlType &type : types())
    {
        out += std::string("control ") + type.name + "\n";
        for (const Property &p : type.properties)
        {
            out += std::string("property ") + p.name + " " +
                   (p.type == Type::String    ? "String"
                    : p.type == Type::Boolean ? "Boolean"
                                              : "Integer") +
                   (p.readOnly   ? " read-only"
                    : p.formOnly ? " form-only"
                                 : "") +
                   "\n";
        }
        for (const Event &e : type.events)
        {
            // The signature without its parentheses: "(ByRef Cancel As Integer)".
            std::string params = e.signature;
            if (params.size() >= 2 && params.front() == '(' && params.back() == ')')
            {
                params = params.substr(1, params.size() - 2);
            }
            out += std::string("event ") + e.name + (params.empty() ? "" : " " + params) + "\n";
        }
    }
    return out;
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
