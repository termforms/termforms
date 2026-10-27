' Run test: an error nobody catches, raised in another module (see
' modules_error.with), is reported with that module's file and line.
Sub Main()
    Dim n As Integer
    n = Divide(1, 0)
End Sub
