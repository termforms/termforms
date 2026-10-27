' Run test: parameters, Functions, recursion, ByRef, string ownership
' across calls, and left-to-right evaluation with side effects.
Begin Form f
    Caption = "Procedures"
    Width = 56
    Height = 10
    Begin TextBox t1
        Width = 50
    End
    Begin TextBox t2
        Top = 1
        Width = 50
    End
    Begin TextBox t3
        Top = 2
        Width = 50
    End
    Begin TextBox t4
        Top = 3
        Width = 50
    End
    Begin TextBox t5
        Top = 4
        Width = 50
    End
    Begin TextBox t6
        Top = 5
        Width = 50
    End
End

Dim calls As Integer
Dim shared As String

Function Factorial(n As Integer) As Integer
    If n <= 1 Then Return 1
    Return n * Factorial(n - 1)
End Function

Function Twice%(n%)
    Twice = n * 2
End Function

Sub Swap(ByRef a As Integer, ByRef b As Integer)
    Dim t As Integer
    t = a
    a = b
    b = t
End Sub

' Changing a ByVal parameter does not reach the caller.
Function Greeting(name As String, polite As Boolean) As String
    If polite Then
        Greeting = "Good day, " & name
        Exit Function
    End If
    name = "you"
    Greeting = "Hi " & name
End Function

Sub Append(ByRef text As String, more As String)
    text = text & more
End Sub

' The argument came from shared; clearing shared must not free it.
Function UseAfterClear(s As String) As String
    shared = ""
    UseAfterClear = s & "!"
End Function

Function Counted() As Integer
    calls = calls + 1
    Return calls
End Function

Function Bump(ByRef n As Integer) As Integer
    n = n + 100
    Bump = 1
End Function

Sub Form_Load()
    Dim x As Integer, y As Integer, name As String, s As String

    t1.Text = Factorial(10) & " " & Twice(21)

    x = 1
    y = 2
    Swap x, y
    s = x & y
    Swap(x, y)
    s = s & " " & x & y
    Call Swap(x, y)
    t2.Text = s & " " & x & y

    name = "Jim"
    t3.Text = Greeting(name, False) & " / " & Greeting(name, True) & " / " & name

    shared = "ab" & "c"   ' built at run time: literals are never freed
    s = "ab"
    Append s, s
    t4.Text = UseAfterClear(shared) & " [" & shared & "] " & s

    ' VB order: calls is read (0) before Counted makes it 1, and so on.
    calls = 0
    x = calls + Counted() * 10 + calls
    t5.Text = x & " " & Counted() & Counted()

    y = 5
    x = y + Bump(y) + y
    t6.Text = x & " " & y
End Sub
