' events.bas -- the demo's event log: every form writes what happened to
' the list on the main form, so each key, click and timer shows its effect.

' Whether GotFocus and LostFocus are logged (View > Show focus events):
' they come with every Tab and click, and can drown the rest.
Public showFocus As Boolean = True

' Adds a line to the log and scrolls to it; the oldest go once it is long.
Sub Note(what As String)
    frmDemo.lstLog.AddItem what
    If frmDemo.lstLog.ListCount > 300 Then
        frmDemo.lstLog.RemoveItem 0
    End If
    frmDemo.lstLog.TopIndex = frmDemo.lstLog.ListCount ' the list keeps it in range
End Sub

Sub FocusNote(what As String)
    If showFocus Then
        Note what
    End If
End Sub
