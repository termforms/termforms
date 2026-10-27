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
        else
        {
            error("unknown key '" + trim(entry.substr(0, equals)) +
                  "' (a project has Name, Startup, Form and Module)");
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
