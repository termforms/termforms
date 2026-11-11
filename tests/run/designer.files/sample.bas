' A sample form for the designer's run test.
Begin Form frmSample
    Caption = "Sample"
    Width = 40
    Height = 12
    Begin Label lblName
        Caption = "&Name:"
        Left = 1
        Top = 1
    End
    ' The name typed.
    Begin TextBox txtName
        Left = 8
        Top = 1
    End
    Begin CommandButton cmdOK
        Caption = "OK"
        Left = 28
        Top = 8
        Width = 8
    End
End

Sub cmdOK_Click()
End Sub
