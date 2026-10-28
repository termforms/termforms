' Run test: DoEvents. A loop in cmdGo_Click calls DoEvents on each pass:
' the first lets the user type into the TextBox, the second lets a Timer
' fire, the third lets cmdGo be pressed again, which runs its handler
' inside itself (re-entry, as VB); then Ctrl+F4 closes the form in the
' middle of the outer loop: Form_Unload runs, the code after DoEvents goes
' on and sees Visible False, and the program ends when it returns. ({IDLE}
' in the key script is where nothing more is waiting.)
Begin Form frmMain
    Caption = "DoEvents"
    Width = 58
    Height = 12
    Begin TextBox txtA
        Width = 10
    End
    Begin CommandButton cmdGo
        Caption = "Go"
        Top = 1
        Width = 8
        Default = True
    End
    Begin Timer tmrTick
        Interval = 100
    End
    Begin Label lblLog
        Top = 3
        Width = 56
        Height = 6
    End
End

Dim busy As Integer

Sub Note(s As String)
    lblLog.Caption = lblLog.Caption & s & " "
End Sub

Sub cmdGo_Click()
    busy = busy + 1
    Dim mine As Integer = busy
    Note "go" & mine
    For i As Integer = 1 To 3
        Note "p" & i
        DoEvents
        If Not Me.Visible Then
            Note "closed:" & mine
            Exit For
        End If
    Next
    Note "done" & mine & "[" & txtA.Text & "]"
End Sub

Sub tmrTick_Timer()
    Note "tick"
    tmrTick.Enabled = False
End Sub

Sub Form_Unload(ByRef Cancel As Integer)
    Note "unload"
End Sub
