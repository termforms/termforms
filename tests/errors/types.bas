' Errors test: Type rules: unknown members and types, records compared or
' joined, a Type containing itself, assigning across Types, members of a
' Function's result, and ByVal arrays of records read-only.
Begin Form f
    Begin CommandButton b
    End
End

Type Node
    Value As Integer
    Child As Node
End Type

Type Point
    X As Integer
    X As Integer
End Type

Type Size
    W As Integer
End Type

Function Origin() As Point
End Function

Sub Look(points() As Point)
    points(0).X = 1
End Sub

Sub Main2()
    Dim p As Point, q As Size, s As String, i As Integer, arr(2) As Point
    p.Z = 1
    p = q
    If p = p Then i = 1
    s = "at " & p
    Origin().X = 3
    i = Point
    Dim r As Pointt
    i = p.X.Y
    arr = p
End Sub
