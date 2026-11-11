' Run test: MouseDown, MouseMove and MouseUp. A click on the button gives
' MouseDown, MouseUp, then Click; a drag that starts on the label is
' followed past its edges (MouseMove and MouseUp keep going to it, with X
' and Y outside it) and is no click; the right button on the form, Ctrl
' with a click in the Frame, the middle button on the button (events but
' no Click), and nothing at all for a disabled TextBox. X and Y count
' from the control's top left, and from the client area's for the form.
' Last, a button whose MouseDown disables it: its MouseUp still comes (it
' has the mouse), but no Click.
Begin Form frmMain
    Caption = "Mouse"
    Width = 50
    Height = 14
    Begin CommandButton cmdGo
        Caption = "Go"
        Width = 8
    End
    Begin CommandButton cmdStop
        Caption = "Stop"
        Left = 10
        Width = 8
    End
    Begin Label lblDrag
        Caption = "Drag me"
        Left = 20
        Width = 10
    End
    Begin TextBox txtOff
        Left = 30
        Top = 1
        Width = 10
        Enabled = False
    End
    Begin Frame fraBox
        Caption = "Box"
        Top = 2
        Width = 16
        Height = 4
    End
    Begin Label lblLog
        Top = 6
        Width = 48
        Height = 6
    End
End

Sub Note(who As String, what As String, Button As Integer, Shift As Integer, X As Integer, Y As Integer)
    lblLog.Caption = lblLog.Caption & who & what & Button & "/" & Shift & " " & X & "," & Y & "  "
End Sub

Sub cmdGo_MouseDown(Button As Integer, Shift As Integer, X As Integer, Y As Integer)
    Note "B", "D", Button, Shift, X, Y
End Sub

Sub cmdGo_MouseUp(Button As Integer, Shift As Integer, X As Integer, Y As Integer)
    Note "B", "U", Button, Shift, X, Y
End Sub

Sub cmdGo_Click()
    lblLog.Caption = lblLog.Caption & "Click  "
End Sub

Sub lblDrag_MouseDown(Button As Integer, Shift As Integer, X As Integer, Y As Integer)
    Note "L", "D", Button, Shift, X, Y
End Sub

Sub lblDrag_MouseMove(Button As Integer, Shift As Integer, X As Integer, Y As Integer)
    Note "L", "M", Button, Shift, X, Y
End Sub

Sub lblDrag_MouseUp(Button As Integer, Shift As Integer, X As Integer, Y As Integer)
    Note "L", "U", Button, Shift, X, Y
End Sub

Sub lblDrag_Click()
    lblLog.Caption = lblLog.Caption & "LClick?  "
End Sub

Sub Form_MouseDown(Button As Integer, Shift As Integer, X As Integer, Y As Integer)
    Note "F", "D", Button, Shift, X, Y
End Sub

Sub Form_MouseUp(Button As Integer, Shift As Integer, X As Integer, Y As Integer)
    Note "F", "U", Button, Shift, X, Y
End Sub

Sub fraBox_MouseDown(Button As Integer, Shift As Integer, X As Integer, Y As Integer)
    Note "R", "D", Button, Shift, X, Y
End Sub

Sub txtOff_MouseDown(Button As Integer, Shift As Integer, X As Integer, Y As Integer)
    Note "T", "D", Button, Shift, X, Y
End Sub

Sub cmdStop_MouseDown(Button As Integer, Shift As Integer, X As Integer, Y As Integer)
    Note "S", "D", Button, Shift, X, Y
    cmdStop.Enabled = False
End Sub

Sub cmdStop_MouseUp(Button As Integer, Shift As Integer, X As Integer, Y As Integer)
    Note "S", "U", Button, Shift, X, Y
End Sub

Sub cmdStop_Click()
    lblLog.Caption = lblLog.Caption & "SClick?  "
End Sub
