' Errors test: file statements: a number where a file name is wanted,
' Line Input into a number, and EOF of a String. (What the parser
' rejects is in files_syntax.bas.)
Begin Form f
    Begin CommandButton b
    End
End

Sub b_Click()
    Dim n As Integer, s As String
    Open 5 For Input As #1
    Line Input #1, n
    n = 0
    If EOF(s) Then n = 1
End Sub
