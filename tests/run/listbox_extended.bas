' Run test: a MultiSelect 2 ListBox chosen by keys and the mouse. Down
' (the first item) then Shift+Down twice selects items 0-2; Ctrl+Down
' twice moves without changing that, and Space adds item 4; Shift+Up
' selects 3-4 only (from the anchor Space set); a click chooses item 0
' alone, then Shift+click on item 2 selects 0-2. Each Click logs the
' selected items and the current one.
Begin Form frmMain
    Caption = "Extended"
    Width = 40
    Height = 14
    Begin ListBox lstDays
        Width = 12
        Height = 8
        MultiSelect = 2
    End
    Begin Label lblLog
        Top = 8
        Width = 38
        Height = 4
    End
End

Sub Form_Load()
    For Each d As String In Split("Sun Mon Tue Wed Thu Fri")
        lstDays.AddItem d
    Next
End Sub

Sub lstDays_Click()
    Dim s As String = ""
    For i As Integer = 0 To lstDays.ListCount - 1
        If lstDays.Selected(i) Then
            s = s & i
        End If
    Next
    lblLog.Caption = lblLog.Caption & s & "@" & lstDays.ListIndex & " "
End Sub
