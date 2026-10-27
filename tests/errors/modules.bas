' Errors test: several modules (see modules.with): a name Public in two
' other modules used unqualified, a Private name used from outside, a
' module with no such name, and a variable named like a module; the
' messages name each file. (A second form is allowed now: two.bas has one.)
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
