' Errors test: loop variables declared by the loop: a suffix contradicting
' As, and As New.
Type Point
    X As Integer
End Type

Sub Main()
    For k% As Long = 1 To 2
    Next
    For Each q As New Point In Split("a")
    Next
End Sub
