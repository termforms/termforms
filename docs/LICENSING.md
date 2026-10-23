# Licensing notes

## The split

| Part | Licence | Why |
| --- | --- | --- |
| Compiler, tools, tests, docs | BSD-2-Clause (`LICENSE`) | Permissive; keeps the copyright notice with source and binaries of TermForms itself |
| Runtime library (`runtime/`) | 0BSD (`runtime/LICENSE`) | Linked into every compiled program, so it must place no obligation on those programs |
| doctest (`third_party/doctest`) | MIT | Used only to build the unit tests; never shipped or linked into programs |

## Unicode width tables (checked 23 Oct 2026)

The terminal runtime needs to know how many cells a character occupies (wide
CJK characters and most emoji take two). The plan is to generate that table
from the Unicode Character Database (UCD), specifically the East_Asian_Width
property and the emoji data files.

The UCD is published under the Unicode License v3. Its one condition is that
the copyright and permission notice appears either **(a)** with all copies of
the data files or software, or **(b)** in associated documentation. The
licence also forbids using the copyright holder's name in advertising without
permission. Source: <https://www.unicode.org/license.txt>.

Assessment (not legal advice):

- A table generated from the UCD is plausibly a copy of derived data, so we
  treat it as covered.
- Condition (b) is met by shipping the Unicode notice in TermForms'
  own documentation (a `THIRD_PARTY_NOTICES` file installed with the runtime).
  It does not require programs built with TermForms to carry the notice, which
  is the property the 0BSD runtime exists to protect.

Decision: generate the tables from the UCD in Phase 4, and add
`runtime/THIRD_PARTY_NOTICES` with the Unicode notice in the same change. The
runtime's own licence stays 0BSD. Revisit only if the Unicode licence changes.
