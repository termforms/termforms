' Run test: the Option-key hint. ˜ (what macOS terminals type for
' Option+N out of the box) on a button shows, once, how to set the
' terminal; the key itself does nothing; typed into a TextBox, ˜ and ¥ are
' text as usual; a second Option character on the button shows nothing
' more. The script ends with the button pressed, after the hint closed.
Begin Form frmMain
    Caption = "Hint"
    Width = 40
    Height = 8
    Begin TextBox txtA
        Width = 20
    End
    Begin CommandButton cmdGo
        Caption = "&Go"
        Top = 2
        Width = 8
    End
    Begin Label lblLog
        Top = 4
        Width = 38
    End
End

Sub cmdGo_Click()
    lblLog.Caption = lblLog.Caption & "go[" & txtA.Text & "] "
End Sub
