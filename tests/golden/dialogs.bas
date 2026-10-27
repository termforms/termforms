' Golden test: MsgBox as a function (its optional parts filled in) and as
' a statement, InputBox, and VB's constants as numbers and text.
Sub Main()
    Dim answer As Integer, name As String
    answer = MsgBox("Save?", vbYesNo + vbQuestion)
    If answer = vbYes Then name = InputBox("Name:", "Save as", "untitled")
    MsgBox "Saved " & name & vbCrLf & "done"
End Sub
