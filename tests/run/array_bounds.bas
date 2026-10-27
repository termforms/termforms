' Run test: an index out of range is error 9, naming the BASIC line.
Begin Form f
    Caption = "Bounds"
    Begin CommandButton b
        Caption = "Go"
    End
End

Sub b_Click()
    Dim a(2) As Integer, i As Integer
    i = 3
    a(i) = 1
End Sub
