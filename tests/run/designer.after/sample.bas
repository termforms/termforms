' A sample form for the designer's run test.
Begin Form frmSample
    Caption = "Sample"
    Height = 12
    Begin CommandButton cmdOK
        Caption = "OK"
        Left = 25
        Top = 7
    End
    ' The name typed.
    Begin TextBox txtName
        Text = "Ann"
        Left = 8
        Top = 1
    End
    Begin Frame fra1
        Caption = "fra1"
        Left = 2
        Top = 3
        Begin CheckBox chk1
            Caption = "chk1"
            Left = 1
        End
        Begin Label lblName
            Caption = "&Name:"
            Left = 1
            Top = 1
        End
    End
End

Sub cmdOK_Click()
End Sub

Sub chk1_Click()
End Sub
