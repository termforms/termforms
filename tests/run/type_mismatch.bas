' Run test: CInt of text that is not a number is error 13 (Val would give 12).
Begin Form f
    Caption = "Convert"
    Begin CommandButton b
        Caption = "Go"
    End
End

Sub b_Click()
    Dim i As Integer
    i = CInt("12abc")
End Sub
