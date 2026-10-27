' Run test: the TextBox's edit buffer. In txtA: Shift+Home selects, typing
' replaces the selection, Ctrl+Left and Ctrl+Right move by words,
' Shift+Ctrl+Left selects a word, Ctrl+X cuts and Ctrl+V pastes it, Ctrl+Z
' undoes a run of typing at once (and again redoes it), Backspace deletes
' the selection. txtMax takes 5 characters at most (MaxLength), however
' many are typed or pasted; txtLocked lets the user copy but not change it;
' txtPass shows * for each character and will not give them out. The
' program sets SelStart, SelLength and SelText, and Change fires for its
' own changes too.
Begin Form frmMain
    Caption = "Edit"
    Width = 58
    Height = 14
    Begin TextBox txtA
        Width = 30
    End
    Begin TextBox txtMax
        Top = 1
        Width = 30
        MaxLength = 5
    End
    Begin TextBox txtLocked
        Top = 2
        Width = 30
        Text = "locked text"
        Locked = True
    End
    Begin TextBox txtPass
        Top = 3
        Width = 30
        PasswordChar = "*"
    End
    Begin CommandButton cmdShow
        Caption = "&Show"
        Top = 4
        Width = 10
    End
    Begin Label lblLog
        Top = 6
        Width = 56
        Height = 6
    End
End

Dim changes As Integer

Sub Note(s As String)
    lblLog.Caption = lblLog.Caption & s & " "
End Sub

Sub txtA_Change()
    changes = changes + 1
End Sub

Sub cmdShow_Click()
    Note "[" & txtA.Text & "] [" & txtMax.Text & "] [" & txtLocked.Text & "] [" & txtPass.Text & "]"
    Note "changes=" & changes
    txtA.Text = "one two three"
    txtA.SelStart = 4
    txtA.SelLength = 3
    Note "sel=" & txtA.SelStart & "," & txtA.SelLength & ":" & txtA.SelText
    txtA.SelText = "2"
    Note "[" & txtA.Text & "] changes=" & changes & " max=" & txtMax.MaxLength & " pw=" & txtPass.PasswordChar
    Try
        txtA.SelStart = -1
    Catch
        Note "E" & Err.Number
    End Try
End Sub
