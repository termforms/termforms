' The second form of the forms golden test.
Begin Form frmOther
    Caption = "Other"
    Begin CommandButton cmdOK
        Caption = "OK"
    End
End

Sub cmdOK_KeyPress(ByRef KeyAscii As Integer)
    If KeyAscii = 13 Then Me.Hide
End Sub
