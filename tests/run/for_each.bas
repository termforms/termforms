' Run test: For Each over Integer and String arrays, a two-dimensional
' array (storage order: the first index changes fastest), an empty dynamic
' array, Types (the variable is a copy) and Refs (shared records), an array
' member through a Ref (kept even when the Ref is changed), a widening loop
' variable, nested loops, Exit For, a ReDim inside that ends it early, and
' error 91 caught (the array is reached through Nothing).
Begin Form f
    Caption = "For Each"
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
End

Type Point
    X As Integer
End Type

Type Bag
    Items() As Integer
End Type

Dim grow() As Integer

Sub Form_Load()
    Dim nums(3) As Integer, words(2) As String, grid(1, 2) As Integer, none() As String
    Dim n As Integer, total As Long, w As String, s As String, i As Integer, j As Integer
    For i = 0 To 3
        nums(i) = i * i
    Next
    words(0) = "a"
    words(1) = "b" & "c"
    words(2) = "d"
    For Each total In nums        ' an Integer widens into a Long
        n = n + CInt(total)
    Next total
    For Each w In words
        s = s & w & "."
    Next
    For Each w In none
        s = s & "never"
    Next
    t1.Text = n & " " & s

    For i = 0 To 1
        For j = 0 To 2
            grid(i, j) = i * 10 + j
        Next
    Next
    s = ""
    For Each n In grid
        s = s & n & ","
        If n = 11 Then Exit For
    Next
    t2.Text = s

    Dim pts(1) As Point, p As Point, refs(1) As Ref Point, r As Ref Point
    pts(1).X = 5
    For Each p In pts
        p.X = p.X + 1             ' a copy: pts is unchanged
    Next
    refs(0) = New Point
    refs(1) = New Point
    For Each r In refs
        r.X = 7                   ' shared: the records change
    Next
    Dim bag As Ref Bag
    bag = New Bag
    ReDim bag.Items(2)
    bag.Items(2) = 4
    s = ""
    For Each n In bag.Items
        bag = Nothing             ' the loop still has the Bag
        s = s & n
    Next
    t3.Text = pts(1).X & " " & p.X & " " & refs(0).X & refs(1).X & " " & s & " " & (bag Is Nothing)

    ReDim grow(4)
    s = ""
    For Each n In grow
        s = s & "x"
        ReDim Preserve grow(1)    ' two left: the loop ends after the second
    Next
    For Each w In words
        For Each n In nums
            If n > 1 Then Exit For
            s = s & w
        Next
    Next
    Try
        For Each n In bag.Items   ' bag is Nothing now
            s = s & "never"
        Next
    Catch When Err.Number = 91
        s = s & " " & Err.Number
    End Try
    t4.Text = s
End Sub
