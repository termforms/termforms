' A form for the designer's renaming test.
Begin Form frmRename
    Caption = "Rename"
    Width = 30
    Height = 8
    Begin CommandButton cmdOK
        Caption = "OK"
        Left = 1
        Top = 1
    End
End

Sub Form_Load()
    cmdOK.Caption = "Ready" ' cmdOK in a comment is not a use
End Sub

Sub cmdOK_Click()
    MsgBox "cmdOK pressed"
End Sub

Sub cmdOK_GotFocus()
    ' nothing yet
End Sub

Sub cmdOKAY()
End Sub
