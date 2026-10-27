' Run test: Long overflow is a runtime error naming the BASIC line.
Begin Form f
    Caption = "Long"
    Begin CommandButton b
        Caption = "Go"
    End
End

Sub b_Click()
    Dim n As Long
    n = 9223372036854775807
    n = n + 1
End Sub
