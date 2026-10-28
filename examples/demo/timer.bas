' timer.bas -- a Timer showing the time, and a long task that keeps the
' form live with DoEvents (Stop works while it runs; a second Run is
' refused rather than started inside the first).
Begin Form frmTimer
    Caption = "Timer and DoEvents"
    Width = 50
    Height = 12
    Begin Timer tmrClock
        Interval = 1000
    End
    Begin Label lblClock
        Left = 1
        Width = 46
    End
    Begin CommandButton cmdRun
        Caption = "&Run a long task"
        Left = 1
        Top = 2
        Width = 20
    End
    Begin CommandButton cmdStop
        Caption = "&Stop"
        Left = 23
        Top = 2
        Width = 10
    End
    Begin Label lblProgress
        Left = 1
        Top = 4
        Width = 46
    End
    Begin CommandButton cmdClose
        Caption = "Close"
        Left = 36
        Top = 8
        Width = 10
        Cancel = True
    End
End

Dim running As Boolean
Dim stopping As Boolean

Sub Form_Load()
    lblClock.Caption = "The time is " & Time$
    lblProgress.Caption = "Not started."
End Sub

Sub tmrClock_Timer()
    lblClock.Caption = "The time is " & Time$
End Sub

Sub cmdRun_Click()
    If running Then
        Note "Run: already running"
        Exit Sub
    End If
    running = True
    stopping = False
    Note "Run: started"
    For pass As Integer = 1 To 40
        lblProgress.Caption = "Working: " & String$(pass, "#") & " " & pass * 100 \ 40 & "%"
        ' A twentieth of a second of "work", the form kept live meanwhile.
        Dim due As Double = Timer + 0.05
        Do While Timer < due
            DoEvents
        Loop
        If stopping Or Not Me.Visible Then
            Exit For
        End If
    Next
    lblProgress.Caption = Pick(stopping, "Stopped.", "Done.")
    Note "Run: " & lblProgress.Caption
    running = False
End Sub

Function Pick(condition As Boolean, yes As String, no As String) As String
    If condition Then
        Return yes
    End If
    Return no
End Function

Sub cmdStop_Click()
    If running Then
        stopping = True
        Note "Stop pressed"
    End If
End Sub

Sub cmdClose_Click()
    stopping = True
    Unload Me
End Sub
