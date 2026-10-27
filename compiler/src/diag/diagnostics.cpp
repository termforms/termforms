// SPDX-License-Identifier: BSD-2-Clause
//
// diagnostics.cpp -- see diagnostics.hpp.

#include "diag/diagnostics.hpp"

namespace tf
{

void Diagnostics::error(Location where, std::string message)
{
    list_.push_back({Severity::Error, where, std::move(message)});
    ++errorCount_;
}

void Diagnostics::warning(Location where, std::string message)
{
    list_.push_back({Severity::Warning, where, std::move(message)});
}

void Diagnostics::print(std::ostream &out) const
{
    for (const Diagnostic &d : list_)
    {
        out << fileName(d.location.file) << ':' << d.location.line << ':' << d.location.column
            << ": " << (d.severity == Severity::Error ? "error" : "warning") << ": " << d.message
            << '\n';
    }
}

} // namespace tf
