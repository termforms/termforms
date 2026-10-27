' Errors test: block structure and declarations of procedures; the parser
' reports each misplaced or missing closer once and carries on.
Function NoType(x)
End Function

Sub Typed() As Integer
End Sub

Sub Main()
    Dim i As Integer
    For i = 1 To 3
        If i = 2 Then
            i = 3
    Next
    Do While i < 5
        i = i + 1
    Loop Until i > 9
    While i > 0
        i = i - 1
    Loop
    Exit Loop
    If i = 1 Then For i = 1 To 2
End Sub

Function Wrong() As Integer
End Sub
