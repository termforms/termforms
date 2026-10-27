// SPDX-License-Identifier: BSD-2-Clause
//
// diagnostics.hpp -- source locations and the diagnostic list.
//
// Every compiler stage reports problems here instead of printing or
// throwing, so stages can keep going after an error (reporting several
// problems per build) and tests can inspect exactly what was reported.

#pragma once

#include <ostream>
#include <string>
#include <vector>

namespace tf
{

// A position in a source file; lines and columns count from 1. file is
// the file's index in the Diagnostics that read it (0 for a single file).
struct Location
{
    int line = 0;
    int column = 0;
    int file = 0;
};

enum class Severity
{
    Warning,
    Error
};

struct Diagnostic
{
    Severity severity;
    Location location;
    std::string message;
};

class Diagnostics
{
public:
    explicit Diagnostics(std::string fileName) { files_.push_back(std::move(fileName)); }

    // Another source file of the program; its Locations carry the index.
    int addFile(std::string fileName)
    {
        files_.push_back(std::move(fileName));
        return static_cast<int>(files_.size()) - 1;
    }

    void error(Location where, std::string message);
    void warning(Location where, std::string message);

    bool hasErrors() const { return errorCount_ > 0; }
    const std::vector<Diagnostic> &all() const { return list_; }
    const std::string &fileName(int file = 0) const
    {
        return files_.at(static_cast<std::size_t>(file));
    }

    // Prints "file:line:col: error: message" lines, in the order reported.
    void print(std::ostream &out) const;

private:
    std::vector<std::string> files_;
    std::vector<Diagnostic> list_;
    int errorCount_ = 0;
};

} // namespace tf
