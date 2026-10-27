' Golden test: Label, CheckBox, OptionButtons in a Frame (each placed in
' it with tf_control_container), Default and Cancel buttons, and Value,
' Enabled and Visible set and read in code.
Begin Form frmMain
    Caption = "Controls"
    Begin Label lblName
        Caption = "&Name:"
    End
    Begin CheckBox chkBold
        Caption = "&Bold"
        Top = 1
        Value = 1
    End
    Begin Frame fraSize
        Caption = "Size"
        Top = 2
        Height = 4
        Begin OptionButton optSmall
            Caption = "&Small"
        End
    End
    Begin CommandButton cmdOK
        Caption = "OK"
        Top = 7
        Default = True
    End
End

Sub cmdOK_Click()
    optSmall.Value = True
    chkBold.Enabled = Not chkBold.Enabled
    If chkBold.Value = 1 And optSmall.Value Then lblName.Visible = False
End Sub
