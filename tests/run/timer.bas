' Run test: Timers, on the key script's clock ({WAIT n} moves it): two
' Timers with different Intervals firing in turn, one stopping itself
' (Enabled = False) and one changed to a new Interval from code, and a
' key typed between waits.
Begin Form frmMain
    Caption = "Timers"
    Width = 50
    Height = 8
    Begin Timer tmrFast
        Interval = 100
    End
    Begin Timer tmrSlow
        Interval = 250
    End
    Begin TextBox txtIn
        Width = 10
    End
    Begin Label lblLog
        Top = 2
        Width = 48
        Height = 3
    End
End

Dim fast As Integer

Sub Note(s As String)
    lblLog.Caption = lblLog.Caption & s & " "
End Sub

Sub tmrFast_Timer()
    fast = fast + 1
    Note "F" & fast
    If fast = 3 Then tmrFast.Enabled = False
End Sub

Sub tmrSlow_Timer()
    Note "S"
    If tmrSlow.Interval = 250 Then tmrSlow.Interval = 1000
End Sub

Sub txtIn_Change()
    Note "k:" & txtIn.Text
End Sub
