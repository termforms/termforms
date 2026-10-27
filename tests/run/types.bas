' Run test: Types as values: copies that do not share (strings, nested
' records, arrays inside), ByVal copies that do not reach the caller,
' ByRef changes that do, arrays of Types with ReDim Preserve, a Function
' returning a Type, and a record copied into an element of itself.
Begin Form f
    Caption = "Types"
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
End

Type Point
    X As Integer
    Y As Integer
End Type

Type Shape
    Name As String
    Corners() As Point
    Origin As Point
    Kids() As Shape
End Type

Function Square(name As String, size As Integer) As Shape
    Square.Name = name
    ReDim Square.Corners(3)
    Square.Corners(1).X = size
    Square.Corners(2).X = size
    Square.Corners(2).Y = size
    Square.Corners(3).Y = size
End Function

Sub Rename(s As Shape)                  ' ByVal: a copy
    s.Name = "renamed"
    s.Corners(1).X = -1
End Sub

Sub Grow(ByRef s As Shape, by As Integer)
    Dim i As Integer
    For i = 0 To UBound(s.Corners)
        s.Corners(i).X = s.Corners(i).X * by
        s.Corners(i).Y = s.Corners(i).Y * by
    Next
End Sub

Function Area(s As Shape) As Integer
    Area = s.Corners(2).X * s.Corners(2).Y
End Function

Sub Form_Load()
    Dim a As Shape, b As Shape, all() As Shape, i As Integer

    a = Square("sq", 2)
    b = a
    b.Name = "copy"
    b.Corners(2).X = 9
    t1.Text = a.Name & " " & a.Corners(2).X & " / " & b.Name & " " & b.Corners(2).X

    Rename a
    t2.Text = a.Name & " " & a.Corners(1).X & " area=" & Area(a)

    Grow a, 3
    t3.Text = "grown area=" & Area(a) & " origin=" & a.Origin.X & "," & a.Origin.Y

    ReDim all(1)
    all(0) = a
    all(1) = Square("small", 1)
    ReDim Preserve all(2)
    all(2).Name = "empty"
    ReDim all(2).Corners(-1)            ' sized, but with no elements
    For i = 0 To 2
        t4.Text = t4.Text & all(i).Name & UBound(all(i).Corners) + 1 & " "
    Next

    ReDim a.Kids(0)
    a.Kids(0) = Square("kid", 5)
    a = a.Kids(0)                      ' a record copied from inside itself
    t5.Text = a.Name & " area=" & Area(a) & " kids=" & (UBound(a.Corners) + 1)
End Sub
