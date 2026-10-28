' Errors test: loop variables declared by the loop: one hiding a local of
' the procedure, one used after its Next, and a Type in a numeric For.
' (The syntax errors are in for_as_syntax.bas.)
Type Point
    X As Integer
End Type

Sub Main()
    Dim i As Integer
    For i As Integer = 1 To 2
    Next
    For j As Integer = 1 To 2
    Next
    j = 3
    For p As Point = 1 To 2
    Next
End Sub
