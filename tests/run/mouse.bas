' Run test: the mouse. In the TextBox a click places the caret, a
' double-click selects a word and a drag selects; a click toggles the
' CheckBox, but a press there released elsewhere does not; clicks select an
' OptionButton and press a button; in the ListBox a click chooses, the
' wheel scrolls, a double-click is DblClick and the arrow in its border
' scrolls back; a click opens the File menu and another chooses New; a
' press outside closes the menus and does nothing more; a Label's Click.
' (Screen 60x14: the form is at column 5, its menu bar on row 1.)
Begin Form frmMain
    Caption = "Mouse"
    Width = 50
    Height = 14
    Begin Menu mnuFile
        Caption = "&File"
        Begin Menu mnuNew
            Caption = "&New"
        End
    End
    Begin TextBox txtA
        Width = 20
        Text = "alpha beta gamma"
    End
    Begin CheckBox chkA
        Caption = "Check"
        Top = 1
        Width = 12
    End
    Begin OptionButton optA
        Caption = "A"
        Top = 2
        Width = 8
        Value = True
    End
    Begin OptionButton optB
        Caption = "B"
        Top = 2
        Left = 14
        Width = 8
    End
    Begin CommandButton cmdGo
        Caption = "Go"
        Top = 3
        Width = 10
    End
    Begin ListBox lstA
        Top = 4
        Width = 16
        Height = 4
    End
    Begin Label lblLog
        Top = 8
        Width = 48
        Height = 3
    End
End

Sub Note(s As String)
    lblLog.Caption = lblLog.Caption & s & " "
End Sub

Sub Form_Load()
    Dim w As String
    For Each w In Split("one two three four five")
        lstA.AddItem w
    Next
End Sub

Sub chkA_Click()
    Note "chk:" & chkA.Value
End Sub

Sub optB_Click()
    Note "optB"
End Sub

Sub cmdGo_Click()
    Note "go[" & txtA.Text & "]"
End Sub

Sub lstA_Click()
    Note "C" & lstA.ListIndex
End Sub

Sub lstA_DblClick()
    Note "D:" & lstA.Text & " top=" & lstA.TopIndex
End Sub

Sub mnuFile_Click()
    Note "file"
End Sub

Sub mnuNew_Click()
    Note "new"
End Sub

Sub lblLog_Click()
    Note "lbl"
End Sub
