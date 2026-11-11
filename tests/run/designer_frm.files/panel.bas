' A form whose block is in its .bas, for the designer to move out.
Begin Form frmPanel
    Caption = "Panel"
    Width = 30
    Height = 8
    Begin CommandButton cmdGo
        Caption = "&Go"
        Left = 1
        Top = 4
    End
End

Sub cmdGo_Click()
    MsgBox "Go"
End Sub
