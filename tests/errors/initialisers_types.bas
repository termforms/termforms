' Errors test: Dim initialisers are checked as assignments, so Option
' Strict's messages apply: a value of the wrong type, at module level and
' in a procedure, and a list item of the wrong type.
Dim bad As Integer = "one"

Sub Main()
    Dim s As String = 1
    Dim n As Integer = 3000000000
    Dim w() As String = {"a", 2}
End Sub
