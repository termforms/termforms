' Run test: arrays in use: values and copies that do not share, a 2-D
' grid, ReDim Preserve, Erase, UBound/LBound, array parameters, a Dim in a
' loop starting fresh, an erased array's UBound (-1), and errors 9 and
' 10 caught, including an element passed ByRef whose array the callee
' tries to ReDim.
Begin Form f
    Caption = "Arrays"
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

Enum Suit
    Hearts
    Spades
End Enum

Dim shared() As Integer

Function Total(values() As Integer) As Integer
    Dim i As Integer
    For i = 0 To UBound(values)
        Total = Total + values(i)
    Next
End Function

Sub Append(ByRef words() As String, word As String)
    ReDim Preserve words(UBound(words) + 1)
    words(UBound(words)) = word
End Sub

Sub Squash(ByRef n As Integer)
    ReDim shared(0)                ' shared is locked while n points into it
    n = 99
End Sub

Sub Form_Load()
    Dim a(4) As Integer, b() As Integer, grid(2, 1) As String, i As Integer, j As Integer
    Dim words() As String, s As String, hands(1) As Suit

    For i = 0 To 4
        a(i) = i * i
    Next
    b = a
    b(0) = 100
    t1.Text = Total(a) & " " & Total(b) & " a(0)=" & a(0) & " ub=" & UBound(b) & " lb=" & LBound(b)

    For i = 0 To 2
        For j = 0 To 1
            grid(i, j) = i & j
        Next
    Next
    ReDim words(0)
    words(0) = "x"
    Append words, "y"
    Append words, "z"
    t2.Text = grid(2, 1) & grid(0, 1) & " " & words(0) & words(1) & words(2) & " ub=" & UBound(grid, 2)

    s = ""
    For i = 1 To 3
        Dim fresh(1) As Integer
        fresh(1) = fresh(1) + i
        s = s & fresh(1)
    Next
    hands(1) = Suit.Spades
    Erase a
    t3.Text = s & " a(4)=" & a(4) & " hand=" & hands(1)

    s = ""
    Try
        i = a(5)
    Catch
        s = Err.Number & " " & Err.Description
    End Try
    Try
        Erase b
        s = s & " / " & UBound(b)
    Catch
        s = s & " / " & Err.Number
    End Try
    t4.Text = s

    ReDim shared(3)
    shared(3) = 1
    Try
        Squash shared(3)
    Catch
        t5.Text = Err.Number & " locked; shared(3)=" & shared(3) & " ub=" & UBound(shared)
    End Try
End Sub
