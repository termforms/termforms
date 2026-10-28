' controls.bas -- Labels, TextBoxes (MaxLength, PasswordChar, Locked),
' CheckBoxes, OptionButtons in two Frames (two groups), Default and Cancel.
Begin Form frmControls
    Caption = "Controls"
    Width = 60
    Height = 18
    Begin Label lblName
        Caption = "&Name:"
        Left = 1
        Top = 1
        Width = 12
    End
    Begin TextBox txtName
        Left = 14
        Top = 1
        Width = 20
        Text = "Ann 安"
    End
    Begin Label lblCode
        Caption = "&Code (digits):"
        Left = 1
        Top = 2
        Width = 13
    End
    Begin TextBox txtCode
        Left = 14
        Top = 2
        Width = 8
        MaxLength = 6
    End
    Begin Label lblPass
        Caption = "&Password:"
        Left = 1
        Top = 3
        Width = 12
    End
    Begin TextBox txtPass
        Left = 14
        Top = 3
        Width = 20
        PasswordChar = "*"
    End
    Begin Label lblLocked
        Caption = "Read only:"
        Left = 1
        Top = 4
        Width = 12
    End
    Begin TextBox txtLocked
        Left = 14
        Top = 4
        Width = 20
        Text = "select and copy me"
        Locked = True
    End
    Begin CheckBox chkBold
        Caption = "&Bold"
        Left = 38
        Top = 1
        Width = 14
    End
    Begin CheckBox chkItalic
        Caption = "&Italic"
        Left = 38
        Top = 2
        Width = 14
        Value = 2
    End
    Begin Frame fraSize
        Caption = "Size"
        Left = 1
        Top = 6
        Width = 18
        Height = 5
        Begin OptionButton optSmall
            Caption = "&Small"
            Left = 1
            Width = 14
        End
        Begin OptionButton optMedium
            Caption = "&Medium"
            Left = 1
            Top = 1
            Width = 14
            Value = True
        End
        Begin OptionButton optLarge
            Caption = "&Large"
            Left = 1
            Top = 2
            Width = 14
        End
    End
    Begin Frame fraColour
        Caption = "Colour"
        Left = 21
        Top = 6
        Width = 18
        Height = 4
        Begin OptionButton optRed
            Caption = "&Red"
            Left = 1
            Width = 14
            Value = True
        End
        Begin OptionButton optGreen
            Caption = "Gr&een"
            Left = 1
            Top = 1
            Width = 14
        End
    End
    Begin CommandButton cmdOK
        Caption = "OK"
        Left = 42
        Top = 12
        Width = 14
        Default = True
    End
    Begin CommandButton cmdCancel
        Caption = "Cancel"
        Left = 42
        Top = 14
        Width = 14
        Cancel = True
    End
    Begin Label lblNote
        Caption = "Enter: OK.  Esc: Cancel.  Alt+letter: the control."
        Left = 1
        Top = 12
        Width = 38
        Height = 3
    End
End

Sub Form_Load()
    Note "frmControls Load"
End Sub

Sub Form_Unload(ByRef Cancel As Integer)
    Note "frmControls Unload"
End Sub

Sub txtName_Change()
    Note "txtName Change: " & txtName.Text
End Sub

Sub txtName_GotFocus()
    FocusNote "txtName GotFocus"
End Sub

Sub txtName_LostFocus()
    FocusNote "txtName LostFocus"
End Sub

' Digits only: anything else is thrown away before the box sees it.
Sub txtCode_KeyPress(ByRef KeyAscii As Integer)
    If KeyAscii >= 32 And (KeyAscii < Asc("0") Or KeyAscii > Asc("9")) Then
        Note "txtCode KeyPress: refused " & Chr$(KeyAscii)
        KeyAscii = 0
    End If
End Sub

Sub txtCode_Change()
    Note "txtCode Change: " & txtCode.Text
End Sub

Sub txtPass_Change()
    Note "txtPass Change: length " & Len(txtPass.Text)
End Sub

Sub txtLocked_KeyDown(ByRef KeyCode As Integer, Shift As Integer)
    Note "txtLocked KeyDown " & KeyCode & " shift " & Shift
End Sub

Sub chkBold_Click()
    Note "chkBold Click: " & chkBold.Value
End Sub

Sub chkItalic_Click()
    Note "chkItalic Click: " & chkItalic.Value
End Sub

Sub optSmall_Click()
    Note "Size: small"
End Sub

Sub optMedium_Click()
    Note "Size: medium"
End Sub

Sub optLarge_Click()
    Note "Size: large"
End Sub

Sub optRed_Click()
    Note "Colour: red"
End Sub

Sub optGreen_Click()
    Note "Colour: green"
End Sub

Sub cmdOK_Click()
    Note "OK: " & txtName.Text & ", code " & txtCode.Text & ", bold " & chkBold.Value
    Unload Me
End Sub

Sub cmdCancel_Click()
    Note "Cancel"
    Unload Me
End Sub
