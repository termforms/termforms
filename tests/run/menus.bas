' Run test: a menu bar. F10 then Down and Enter chooses File > New; Alt+E
' opens Edit (its Click fires first and checks Wrap); Ctrl+S is Save's
' Shortcut, working with the menu closed; Ctrl+B is F10's alternative;
' Alt+O is the Options menu's access key, ahead of the button's &Options;
' Esc goes back to the bar and again leaves it; Left goes round the end of
' the bar; Down passes over the separator and the disabled Print. The
' script ends with File open over the form. In Form_Load, a Shortcut set
' in code is shown as Windows writes it, and a bad one is error 380.
Begin Form frmMain
    Caption = "Menus"
    Width = 58
    Height = 14
    Begin Menu mnuFile
        Caption = "&File"
        Begin Menu mnuNew
            Caption = "&New"
            Shortcut = "Ctrl+N"
        End
        Begin Menu mnuSave
            Caption = "&Save"
            Shortcut = "Ctrl+S"
        End
        Begin Menu mnuSep
            Caption = "-"
        End
        Begin Menu mnuPrint
            Caption = "&Print"
            Enabled = False
        End
        Begin Menu mnuQuit
            Caption = "&Quit"
            Shortcut = "shift+f5"
        End
    End
    Begin Menu mnuEdit
        Caption = "&Edit"
        Begin Menu mnuWrap
            Caption = "&Wrap"
        End
    End
    Begin Menu mnuOptions
        Caption = "&Options"
    End
    Begin CommandButton cmdOptions
        Caption = "&Options"
        Width = 12
    End
    Begin Label lblLog
        Top = 7
        Width = 56
        Height = 3
    End
End

Sub Note(s As String)
    lblLog.Caption = lblLog.Caption & s & " "
End Sub

Sub Form_Load()
    mnuNew.Shortcut = "ctrl+n"
    Try
        mnuSave.Shortcut = "Ctrl+Space"
    Catch
        Note "bad:" & Err.Number & ":" & mnuSave.Shortcut
    End Try
End Sub

Sub mnuFile_Click()
    Note "file"
End Sub

Sub mnuNew_Click()
    Note "new"
End Sub

Sub mnuSave_Click()
    Note "save"
End Sub

Sub mnuQuit_Click()
    Note "quit"
End Sub

Sub mnuEdit_Click()
    mnuWrap.Checked = True
    Note "edit"
End Sub

Sub mnuWrap_Click()
    Note "wrap:" & mnuWrap.Checked
End Sub

Sub mnuOptions_Click()
    Note "options"
End Sub

Sub cmdOptions_Click()
    Note "button"
End Sub
