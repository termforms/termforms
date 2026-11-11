' Run test: a multi-line TextBox (no scroll bars, so it wraps). Enter
' starts a new line rather than pressing the Default button; a long line
' wraps at a space (a space that does not fit hangs at the row's end);
' Up and Down keep the column they started from, even across a shorter
' row; Home and End go to the row's ends, Ctrl+End to the text's;
' Shift+Up selects across rows; a paste keeps its line breaks (CR LF
' becomes LF) and tabs, where the single-line box makes them spaces; text
' the program sets has its CR LF and CR made LF. PasswordChar is ignored
' in a multi-line box, as VB.
Begin Form frmMain
    Caption = "Notes"
    Width = 60
    Height = 18
    Begin TextBox txtNotes
        Width = 20
        Height = 6
        MultiLine = True
        PasswordChar = "*"
    End
    Begin TextBox txtCode
        Left = 22
        Width = 20
        Height = 3
        MultiLine = True
    End
    Begin TextBox txtOne
        Top = 7
        Width = 20
    End
    Begin CommandButton cmdShow
        Caption = "&Show"
        Top = 7
        Left = 22
        Width = 10
        Default = True
    End
    Begin Label lblLog
        Top = 9
        Width = 58
        Height = 7
    End
End

Dim shown As Integer

Function Visible(s As String) As String
    Return Replace(Replace(s, vbLf, "|"), vbTab, "^")
End Function

Sub Form_Load()
    txtCode.Text = "set" & vbCrLf & "by code" & vbCr & "end"
End Sub

Sub cmdShow_Click()
    shown = shown + 1
    lblLog.Caption = lblLog.Caption & "show " & shown & " [" & Visible(txtNotes.Text) & "] [" & Visible(txtOne.Text) & "] sel=" & txtNotes.SelStart & "," & txtNotes.SelLength & " ml=" & txtNotes.MultiLine & " code=" & Len(txtCode.Text) & " [" & Visible(txtCode.Text) & "] "
End Sub
