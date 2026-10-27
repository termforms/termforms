' Errors test: semantic rules for parameters, calls, Functions, loops,
' Exit/Return and block scope, all reported in one build.
Begin Form f
    Begin CommandButton b
    End
End

Dim g As Integer

Function Area(w As Integer, h As Integer) As Integer
    Area = w * h
End Function

Sub Show(ByRef n As Integer, text As String)
End Sub

Function NoArgs() As String
    NoArgs = "x"
End Function

Sub b_Click(x As Integer)
End Sub

Sub Main2()
    Dim s As String, k As Integer
    k = Area(1)
    k = Area(1, "2")
    Show 1, "a"
    Show k, 2
    Show g, s
    Show s, s
    s = Show(k, s)
    NoArgs = "y"
    k = area(1, 2)
    k(1)
    For s = 1 To 3
    Next
    For k = 1 To "3"
    Next j
    Do While k
    Loop
    Exit For
    Exit Function
    Return 1
    If k = 1 Then
        Dim inner As Integer
    End If
    inner = 2
    Dim k As Integer
End Sub

Function NoReturn() As Integer
    Return
    Exit Sub
    Dim NoReturn As Integer
    NoReturn = "text"
End Function
