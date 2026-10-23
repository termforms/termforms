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
    static const std::vector<ControlType> kTypes = {
        {"Form",
         "TF_CTL_FORM",
         {{"Caption", Type::String, "TF_PROP_CAPTION"},
          {"Width", Type::Integer, "TF_PROP_WIDTH"},
          {"Height", Type::Integer, "TF_PROP_HEIGHT"}},
         {{"Load", EventKind::Load, nullptr}}},
        {"TextBox",
         "TF_CTL_TEXTBOX",
         {{"Text", Type::String, "TF_PROP_TEXT"},
          {"Left", Type::Integer, "TF_PROP_LEFT"},
          {"Top", Type::Integer, "TF_PROP_TOP"},
          {"Width", Type::Integer, "TF_PROP_WIDTH"},
          {"Height", Type::Integer, "TF_PROP_HEIGHT"}},
         {{"Change", EventKind::Change, "TF_EVENT_CHANGE"}}},
        {"CommandButton",
         "TF_CTL_COMMANDBUTTON",
         {{"Caption", Type::String, "TF_PROP_CAPTION"},
          {"Left", Type::Integer, "TF_PROP_LEFT"},
          {"Top", Type::Integer, "TF_PROP_TOP"},
          {"Width", Type::Integer, "TF_PROP_WIDTH"},
          {"Height", Type::Integer, "TF_PROP_HEIGHT"}},
         {{"Click", EventKind::Click, "TF_EVENT_CLICK"}}},
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
