' Golden test: a Timer made with its Interval, its Timer event wired, and
' Interval and Enabled set from code.
Begin Form frmMain
    Caption = "Timer"
    Begin Timer tmr
        Interval = 500
    End
End

Sub tmr_Timer()
    tmr.Interval = tmr.Interval * 2
    If tmr.Interval > 4000 Then tmr.Enabled = False
End Sub
