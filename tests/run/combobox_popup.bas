' Run test: one popup at a time. The ComboBox near the bottom of the
' screen opens its list upwards (no room below), over the menu bar. F10
' with the list open closes it (choosing nothing) and goes to the menu
' bar; Esc leaves the bar. A click on the arrow opens the list again and
' a click elsewhere closes it, doing nothing more (the button is not
' pressed). Tab with the list open chooses its highlighted item and moves
' on. Last the list is open again, upwards, for the dump.
Begin Form frmMain
    Caption = "Popups"
    Width = 40
    Height = 8
    Begin Menu mnuFile
        Caption = "&File"
        Begin Menu mnuQuit
            Caption = "&Quit"
        End
    End
    Begin Label lblLog
        Left = 14
        Width = 24
        Height = 3
    End
    Begin ComboBox cboSize
        Top = 4
        Width = 12
        Style = 2
    End
    Begin CommandButton cmdGo
        Caption = "&Go"
        Left = 20
        Top = 4
        Width = 8
    End
End

Sub Note(s As String)
    lblLog.Caption = lblLog.Caption & s & " "
End Sub

Sub Form_Load()
    cboSize.AddItem "small"
    cboSize.AddItem "medium"
    cboSize.AddItem "large"
    cboSize.ListIndex = 0
End Sub

Sub cboSize_Click()
    Note "C:" & cboSize.Text
End Sub

Sub cboSize_DropDown()
    Note "D"
End Sub

Sub mnuFile_Click()
    Note "menu"
End Sub

Sub cmdGo_Click()
    Note "go!"
End Sub

Sub cmdGo_GotFocus()
    Note "focus:go"
End Sub
