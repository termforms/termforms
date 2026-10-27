' Run test: Label, CheckBox, OptionButton and Frame, and the keys: typing
' then Enter (the Default button), Tab skipping a disabled CheckBox and
' stopping once in each OptionButton group, an arrow moving within the
' Frame's group, Space toggling and selecting, Esc (the Cancel button),
' Alt+letter access keys (a Label's to the control after it), and
' Ctrl+F4 closing. Each handler adds to the
' log Label, so the screen shows what happened and in what order.
Begin Form frmMain
    Caption = "Controls"
    Width = 50
    Height = 14
    Begin Label lblName
        Caption = "&Name:"
        Width = 7
    End
    Begin TextBox txtName
        Left = 8
        Width = 20
    End
    Begin CheckBox chkBold
        Caption = "&Bold"
        Top = 1
        Width = 12
    End
    Begin CheckBox chkOff
        Caption = "Off"
        Top = 1
        Left = 14
        Width = 10
        Enabled = False
    End
    Begin Frame fraSize
        Caption = "Size"
        Top = 2
        Width = 18
        Height = 4
        Begin OptionButton optSmall
            Caption = "&Small"
            Width = 12
        End
        Begin OptionButton optLarge
            Caption = "&Large"
            Top = 1
            Width = 12
        End
    End
    Begin OptionButton optRed
        Caption = "Red"
        Top = 2
        Left = 20
        Width = 10
    End
    Begin OptionButton optBlue
        Caption = "Blue"
        Top = 3
        Left = 20
        Width = 10
    End
    Begin CommandButton cmdOK
        Caption = "OK"
        Top = 7
        Width = 10
        Default = True
    End
    Begin CommandButton cmdCancel
        Caption = "Cancel"
        Top = 7
        Left = 12
        Width = 12
        Cancel = True
    End
    Begin Label lblHidden
        Caption = "never shown"
        Top = 8
        Width = 20
        Visible = False
    End
    Begin Label lblLog
        Top = 9
        Width = 48
        Height = 2
    End
End

Sub Note(s As String)
    lblLog.Caption = lblLog.Caption & s & " "
End Sub

Sub Form_Load()
    optSmall.Value = True         ' fires Click, as VB
End Sub

Sub chkBold_Click()
    Note "B" & chkBold.Value
End Sub

Sub optSmall_Click()
    Note "S"
End Sub

Sub optLarge_Click()
    Note "L"
End Sub

Sub optRed_Click()
    Note "R"
End Sub

Sub optBlue_Click()
    Note "U"
End Sub

Sub cmdOK_Click()
    Note "OK:" & txtName.Text
End Sub

Sub cmdCancel_Click()
    Note "C"
End Sub
