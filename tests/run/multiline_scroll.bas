' Run test: scroll bars on multi-line TextBoxes. txtLog (ScrollBars 2)
' wraps and scrolls down: text set by the program puts the caret at the
' end, so it shows the last rows, then the wheel moves the rows (not the
' caret) up three. txtWide (ScrollBars 3) does not wrap: a press on its
' down bar focuses it, Ctrl+Home, Down and End take the caret to the end
' of the long line and the view scrolls across to keep it in sight; Up
' keeps the column (End's column, past the shorter line, comes back to
' its end). The thumbs show where each view is.
Begin Form frmMain
    Caption = "Scroll"
    Width = 50
    Height = 14
    Begin TextBox txtLog
        Width = 20
        Height = 5
        MultiLine = True
        ScrollBars = 2
    End
    Begin TextBox txtWide
        Left = 22
        Width = 16
        Height = 4
        MultiLine = True
        ScrollBars = 3
    End
    Begin Label lblLog
        Top = 6
        Width = 48
        Height = 5
    End
End

Sub Form_Load()
    Dim s As String = ""
    For i As Integer = 1 To 30
        s = s & "line " & i & vbLf
    Next
    txtLog.Text = s & "the last one is long enough to wrap"
    txtWide.Text = "short" & vbLf & "a much longer line that scrolls across" & vbLf & "end"
End Sub

Sub txtWide_KeyDown(ByRef KeyCode As Integer, Shift As Integer)
    lblLog.Caption = lblLog.Caption & KeyCode & "@" & txtWide.SelStart & " "
End Sub
