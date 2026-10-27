' Run test: ListBoxes: a Sorted one (AddItem in order; at an index when
' given), arrows, a letter jumping to an item, Enter as DblClick, End; a
' MultiSelect one too short for its items (scrolling, the arrows in its
' border), the caret moved and Space toggling; then code reading and
' changing items: List(i), Selected(i), RemoveItem, ListCount, SelCount.
' A ListIndex out of range is error 380, caught by a Try (E380).
Begin Form frmMain
    Caption = "Lists"
    Width = 50
    Height = 14
    Begin ListBox lstFruit
        Width = 14
        Height = 7
        Sorted = True
    End
    Begin ListBox lstPick
        Left = 16
        Width = 12
        Height = 4
        MultiSelect = 1
    End
    Begin CommandButton cmdShow
        Caption = "Show"
        Left = 30
        Width = 10
    End
    Begin Label lblLog
        Top = 8
        Width = 48
        Height = 4
    End
End

Sub Note(s As String)
    lblLog.Caption = lblLog.Caption & s & " "
End Sub

Sub Form_Load()
    lstFruit.AddItem "pear"
    lstFruit.AddItem "Apple"
    lstFruit.AddItem "fig"
    lstFruit.AddItem "banana"
    lstFruit.AddItem "kiwi", 0      ' at a given place, even when Sorted
    Dim w As String
    For Each w In Split("one two three four five")
        lstPick.AddItem w
    Next
    Try
        lstFruit.ListIndex = 9
    Catch
        Note "E" & Err.Number
    End Try
End Sub

Sub lstFruit_Click()
    Note "C" & lstFruit.ListIndex
End Sub

Sub lstFruit_DblClick()
    Note "D:" & lstFruit.Text
End Sub

Sub lstPick_Click()
    Note "P" & lstPick.SelCount
End Sub

Sub cmdShow_Click()
    Dim i As Integer, s As String
    For i = 0 To lstPick.ListCount - 1
        If lstPick.Selected(i) Then s = s & lstPick.List(i) & ","
    Next
    lstFruit.List(0) = "KIWI"
    lstFruit.RemoveItem 1
    lstPick.Selected(4) = True
    Note "[" & s & "] " & lstFruit.List(0) & lstFruit.ListCount & " " & lstFruit.Text & lstPick.SelCount
End Sub
