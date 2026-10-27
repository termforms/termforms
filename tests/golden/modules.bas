' Golden test: two modules (see modules.with): a Public variable and Sub
' of the other module, the same Private name in both (C names given the
' module), and statements marked with their file (TF_AT).
Sub Main()
    count = 1
    Bump
    helper = 2
End Sub

Dim helper As Integer
