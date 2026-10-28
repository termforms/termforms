' Run test: loop variables declared by the loop. For i As Integer and For
' Each w As String; the same name in two loops one after the other; a
' Long counter and a Type element; nested loops each with their own.
Begin Form frmMain
    Caption = "For As"
    Width = 58
    Height = 8
    Begin Label lblLog
        Width = 56
        Height = 6
    End
End

Type Point
    X As Integer
End Type

Sub Note(s As String)
    lblLog.Caption = lblLog.Caption & s & " "
End Sub

Sub Form_Load()
    For i As Integer = 1 To 3
        Note "i" & i
    Next i
    For i As Long = 3000000000 To 3000000001
        Note "L" & i
    Next
    For Each w As String In Split("a b")
        Note "w" & w
    Next w
    Dim pts(1) As Point
    pts(0).X = 7
    pts(1).X = 8
    For Each p As Point In pts
        Note "p" & p.X
    Next
    For r As Integer = 1 To 2
        For c As Integer = 1 To 2
            Note r & "," & c
        Next
    Next
End Sub
