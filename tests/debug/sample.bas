' Debugger test program (tests/tools/tf_debug.py): Strings, arrays of one
' and two dimensions, a Type, a Dictionary, an array of Refs, and a
' Function to step into. The test stops at the line marked STOP.
Type Point
    X As Integer
    Name As String
End Type

Function Twice(n As Integer) As Integer
    Twice = n * 2
End Function

Sub Main()
    Dim s As String = "hé" & "llo"
    Dim a() As Integer = {1, 2, 3}
    Dim g(,) As Integer = {{1, 2}, {3, 4}}
    Dim p As Point
    p.X = 7
    p.Name = "pt" & "1"
    Dim d As Dictionary(Of Integer)
    d("k" & "1") = 5
    Dim refs(0) As Ref Point
    refs(0) = New Point
    refs(0).X = 9
    Dim n As Integer
    n = Twice(Len(s)) ' STOP
    n = n + 1
End Sub
