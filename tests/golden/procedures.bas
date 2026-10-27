' Golden test: parameters (ByVal, ByRef), Functions returning by name and
' by Return, recursion, call syntaxes, Exit Sub/Function and cleanup.
Begin Form frmMain
    Caption = "Procedures"
    Begin TextBox txtOut
        Width = 30
    End
End

Dim calls As Integer

Function Twice(n As Integer) As Integer
    Twice = n * 2
End Function

Function Factorial(ByVal n As Integer) As Integer
    If n <= 1 Then Return 1
    Return n * Factorial(n - 1)
End Function

Function Greeting$(name As String, polite As Boolean)
    If polite Then
        Greeting = "Good day, " & name
        Exit Function
    End If
    name = "you"
    Greeting = "Hi " & name
End Function

Sub Swap(ByRef a As Integer, ByRef b As Integer)
    Dim t As Integer
    t = a
    a = b
    b = t
End Sub

Sub Append(ByRef text As String, ByVal more As String)
    If more = "" Then Exit Sub
    text = text & more
End Sub

Function Counted() As Integer
    calls = calls + 1
    Counted = calls
End Function

Sub Form_Load()
    Dim x As Integer, y As Integer, s As String
    x = 1
    y = Twice(x) + Counted
    Swap x, y
    Swap(x, y)
    Call Swap(x, y)
    s = Greeting(txtOut.Text & "!", x > y)
    Append s, "."
    txtOut.Text = s & Factorial(5)
End Sub
