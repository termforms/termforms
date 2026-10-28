' Errors test: a Declare's Lib, checked once the program is otherwise
' right: a C file that is not there (looked for next to this file), and a
' Lib that is neither a library's name nor a C file.
Declare Function missing Lib "no_such_file.c" () As Integer
Declare Function badlib Lib "my lib" () As Integer

Sub Main()
End Sub
