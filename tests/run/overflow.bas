' Run test: Integer overflow is a runtime error naming the BASIC line,
' and the screen is restored before the message is printed.
Begin Form f
    Caption = "Overflow"
    Begin CommandButton b
        Caption = "Boom"
    End
End

Sub b_Click()
    Dim n As Integer
    n = 2147483647
    n = n + 1
End Sub
