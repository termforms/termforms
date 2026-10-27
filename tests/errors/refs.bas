' Errors test: Ref rules: Ref of a non-Type, New of a non-Type, comparing
' Refs with = rather than Is, Is on values, Refs of different Types, and
' Nothing used as a record.
Begin Form f
    Begin CommandButton b
    End
End

Enum Color
    Red
End Enum

Type Node
    Value As Integer
End Type

Type Other
    Value As Integer
End Type

Sub Main2()
    Dim a As Ref Node, other2 As Ref Node, o As Ref Other, v As Node, i As Integer
    Dim c As Ref Color
    a = New Color
    If a = other2 Then i = 1
    If i Is v Then i = 2
    a = o
    v = Nothing
    If a Is o Then i = 3
    i = a
End Sub
