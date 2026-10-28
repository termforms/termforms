' Errors test: Declare syntax: a Declare without Lib, a Lib that is not in
' quotes, and an Alias without its name.
Declare Function one () As Integer
Declare Function two Lib sqlite3 () As Integer
Declare Function three Lib "" Alias () As Integer
