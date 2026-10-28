' The dialog for afresh.bas.
Begin Form frmAsk
    Caption = "Ask"
    Width = 30
    Height = 9
    Begin TextBox txtA
        Width = 12
        Text = "start"
    End
    Begin CheckBox chkA
        Caption = "Check"
        Top = 1
        Width = 12
    End
    Begin ListBox lstA
        Top = 2
        Width = 12
        Height = 3
    End
    Begin CommandButton cmdOK
        Caption = "OK"
        Top = 5
        Width = 8
        Default = True
    End
End

Sub Form_Load()
    lstA.AddItem "item"
    frmMain.lblLog.Caption = frmMain.lblLog.Caption & "load:" & txtA.Text & " "
End Sub

Sub cmdOK_Click()
    Me.Caption = "Changed"
    Unload Me
End Sub
