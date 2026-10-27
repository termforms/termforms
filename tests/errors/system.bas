' Errors test: system functions: Environ$ of a number, Shell of a number,
' an argument to Now, and Timer (a Double) into an Integer.
Begin Form f
    Begin CommandButton b
    End
End

Sub b_Click()
    Dim s As String, n As Integer
    s = Environ$(1)
    n = Shell(2)
    s = Now(1)
    n = Timer
End Sub
