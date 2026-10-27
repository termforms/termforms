' Errors test: file-system routines: a statement used as a function, a
' number where a path is wanted, and too many arguments.
Begin Form f
    Begin CommandButton b
    End
End

Sub b_Click()
    Dim n As Integer, s As String
    n = Kill("a.txt")
    MkDir 5
    s = Dir$("*", 2)
    FileCopy "a"
End Sub
