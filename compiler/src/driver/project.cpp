// SPDX-License-Identifier: BSD-2-Clause
//
// project.cpp -- see project.hpp.

#include "driver/project.hpp"

#include <cctype>
#include <sstream>

namespace tf::driver
{

namespace
{

std::string trim(const std::string &s)
{
    std::size_t from = 0;
    std::size_t to = s.size();
    while (from < to && std::isspace(static_cast<unsigned char>(s[from])) != 0)
    {
        ++from;
    }
    while (to > from && std::isspace(static_cast<unsigned char>(s[to - 1])) != 0)
    {
        --to;
    }
    return s.substr(from, to - from);
}

std::string lower(std::string s)
{
    for (char &c : s)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

// The roles forms are drawn in, as a project names them (FormBackColor=)
// and as the runtime does (tf_colour_role in tf_runtime.h): keep the two
// lists alike.
struct ColourRole
{
    const char *name;
    const char *role;
};

constexpr ColourRole kColourRoles[] = {
    {"Backdrop", "TF_COLOUR_BACKDROP"},
    {"FormBack", "TF_COLOUR_FORM_BACK"},
    {"FormText", "TF_COLOUR_FORM_TEXT"},
    {"FormDisabled", "TF_COLOUR_FORM_DISABLED"},
    {"FormKey", "TF_COLOUR_FORM_KEY"},
    {"DialogBack", "TF_COLOUR_DIALOG_BACK"},
    {"DialogText", "TF_COLOUR_DIALOG_TEXT"},
    {"DialogDisabled", "TF_COLOUR_DIALOG_DISABLED"},
    {"DialogKey", "TF_COLOUR_DIALOG_KEY"},
    {"MenuBack", "TF_COLOUR_MENU_BACK"},
    {"MenuText", "TF_COLOUR_MENU_TEXT"},
    {"MenuDisabled", "TF_COLOUR_MENU_DISABLED"},
    {"MenuKey", "TF_COLOUR_MENU_KEY"},
    {"ButtonBack", "TF_COLOUR_BUTTON_BACK"},
    {"ButtonText", "TF_COLOUR_BUTTON_TEXT"},
    {"ButtonDisabled", "TF_COLOUR_BUTTON_DISABLED"},
    {"ButtonKey", "TF_COLOUR_BUTTON_KEY"},
    {"HighlightBack", "TF_COLOUR_HIGHLIGHT_BACK"},
    {"HighlightText", "TF_COLOUR_HIGHLIGHT_TEXT"},
    {"InactiveBack", "TF_COLOUR_INACTIVE_BACK"},
    {"InactiveText", "TF_COLOUR_INACTIVE_TEXT"},
    {"FieldBack", "TF_COLOUR_FIELD_BACK"},
    {"FieldText", "TF_COLOUR_FIELD_TEXT"},
};

// "#RRGGBB" as 0xRRGGBB; false if it is not written so.
bool parseColour(const std::string &value, unsigned long &rgb)
{
    if (value.size() != 7 || value[0] != '#')
    {
        return false;
    }
    rgb = 0;
    for (std::size_t i = 1; i < value.size(); ++i)
    {
        const char c = value[i];
        if (std::isxdigit(static_cast<unsigned char>(c)) == 0)
        {
            return false;
        }
        rgb = rgb * 16 + static_cast<unsigned long>(std::isdigit(static_cast<unsigned char>(c)) != 0
                                                        ? c - '0'
                                                        : std::tolower(c) - 'a' + 10);
    }
    return true;
}

} // namespace

Project parseProject(const std::string &text)
{
    Project project;
    std::istringstream in(text);
    std::string raw;
    int line = 0;
    bool named = false;
    auto error = [&](const std::string &message) {
        project.errors.push_back("line " + std::to_string(line) + ": " + message);
    };
    while (std::getline(in, raw))
    {
        ++line;
        const std::string entry = trim(raw);
        if (entry.empty() || entry[0] == '\'')
        {
            continue;
        }
        const std::size_t equals = entry.find('=');
        if (equals == std::string::npos)
        {
            error("expected Key=Value, as in Module=stock.bas");
            continue;
        }
        const std::string key = lower(trim(entry.substr(0, equals)));
        const std::string value = trim(entry.substr(equals + 1));
        if (value.empty())
        {
            error("'" + trim(entry.substr(0, equals)) + "' needs a value");
            continue;
        }
        if (key == "form" || key == "module")
        {
            project.files.push_back({value, key == "form", line});
        }
        else if (key == "name")
        {
            if (named)
            {
                error("Name is given twice");
            }
            named = true;
            project.name = value;
        }
        else if (key == "startup")
        {
            if (project.startupLine != 0)
            {
                error("Startup is given twice");
            }
            project.startup = value;
            project.startupLine = line;
        }
        else if (key.size() > 5 && key.compare(key.size() - 5, 5, "color") == 0)
        {
            const std::string written = trim(entry.substr(0, equals));
            const std::string name = key.substr(0, key.size() - 5);
            const ColourRole *found = nullptr;
            for (const ColourRole &r : kColourRoles)
            {
                found = lower(r.name) == name ? &r : found;
            }
            unsigned long rgb = 0;
            if (found == nullptr)
            {
                std::string names;
                for (const ColourRole &r : kColourRoles)
                {
                    names += std::string(names.empty() ? "" : ", ") + r.name + "Color";
                }
                error("unknown colour '" + written + "' (the colours are " + names + ")");
            }
            else if (!parseColour(value, rgb))
            {
                error(written + " needs a colour written #RRGGBB, as in #0000AA");
            }
            else
            {
                bool repeated = false;
                for (const ProjectColour &c : project.colours)
                {
                    repeated = repeated || c.role == found->role;
                }
                if (repeated)
                {
                    error(written + " is given twice");
                }
                project.colours.push_back({found->role, rgb, line});
            }
        }
        else
        {
            error("unknown key '" + trim(entry.substr(0, equals)) +
                  "' (a project has Name, Startup, Form, Module and colours such as "
                  "FormBackColor)");
        }
    }
    if (project.files.empty() && project.errors.empty())
    {
        line = 1;
        error("the project lists no files (Form= or Module=)");
    }
    return project;
}

} // namespace tf::driver
