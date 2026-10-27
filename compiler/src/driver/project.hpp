// SPDX-License-Identifier: BSD-2-Clause
//
// project.hpp -- the .vbp project file: VB6-style Key=Value lines naming a
// program's files (settled with Jim, 28 Oct 2026). Pure: the caller reads
// the file and resolves the paths, so this is unit-tested on its own.

#pragma once

#include <string>
#include <vector>

namespace tf::driver
{

struct ProjectFile
{
    std::string path; // as written: relative to the .vbp's folder, or absolute
    bool isForm = false;
    int line = 0; // where it is listed, for messages
};

struct Project
{
    std::string name;    // Name=, or empty
    std::string startup; // Startup=: a form's name or "Sub Main"; empty if not given
    int startupLine = 0;
    std::vector<ProjectFile> files;  // Form= and Module=, in order
    std::vector<std::string> errors; // "line N: message", in order
    bool ok() const { return errors.empty(); }
};

// Keys are Name, Startup, Form and Module (any case); values are trimmed;
// blank lines and lines starting with ' are ignored. An unknown key, a
// repeated Name or Startup, a line without '=' or a project with no files
// is an error.
Project parseProject(const std::string &text);

} // namespace tf::driver
