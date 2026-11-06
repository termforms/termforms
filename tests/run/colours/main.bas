' A form drawn in a project's own colours (colours.vbp). The headless
' screen is text, so this checks that the colour lines compile and run.
Begin Form frmColours
    Caption = "Colours"
    Width = 30
    Height = 6
    Begin Label lblName
        Caption = "&Name:"
        Left = 1
    End
    Begin TextBox txtName
        Left = 8
        Width = 19
    End
    Begin CommandButton cmdOK
        Caption = "OK"
        Left = 8
        Top = 2
        Width = 10
        Default = True
    End
End

Sub cmdOK_Click()
    lblName.Caption = "Done"
End Sub
