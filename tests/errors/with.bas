' Errors test: With rules: a .Member outside any With, With on something
' that has no members (a number, an array, Nothing), a member the Type does
' not have, and changing a With on a Function's Type result (a copy).
Begin Form f
    Begin CommandButton b
    End
End

Type Item
    Name As String
    Tags() As String
End Type

Function Make() As Item
    Make.Name = "x"
End Function

Sub Bump(ByRef s As String)
    s = s & "!"
End Sub

Sub b_Click()
    Dim n As Integer, items(2) As Item, it As Item
    .Name = "a"
    With n
        .Name = "b"
    End With
    With items
    End With
    With Nothing
        .Name = "c"
    End With
    With it
        .Nmae = "d"
        n = UBound(.Tags)
    End With
    With Make()
        it.Name = .Name
        .Name = "e"
        Bump .Name
        ReDim .Tags(1)
    End With
End Sub
