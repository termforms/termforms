' Errors test: several modules (see modules.with): a name Public in two
' other modules used unqualified, a Private name used from outside, a
' module with no such name, a variable named like a module, and a second
' form; the messages name each file.
Begin Form f
    Begin CommandButton b
    End
End

Sub b_Click()
    Dim n As Integer
    n = Shared()
    n = one.Secret()
    n = one.Missing
    n = two.count
End Sub
