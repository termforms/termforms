' The second form of the forms run test.
Begin Form frmOptions
    Caption = "Options"
    Width = 30
    Height = 6
    Begin CheckBox chkSound
        Caption = "Sound"
        Width = 12
    End
    Begin CommandButton cmdOK
        Caption = "OK"
        Top = 2
        Width = 8
        Default = True
    End
End

Public chosen As Integer

Sub Form_Load()
    frmMain.lblLog.Caption = frmMain.lblLog.Caption & "oL "
End Sub

Sub Form_Activate()
    frmMain.lblLog.Caption = frmMain.lblLog.Caption & "oA "
End Sub

Sub chkSound_KeyDown(ByRef KeyCode As Integer, Shift As Integer)
    If KeyCode = vbKeyF2 Then frmMain.lblLog.Caption = frmMain.lblLog.Caption & "F2 "
End Sub

Sub cmdOK_Click()
    chosen = chkSound.Value
    Me.Hide
End Sub
