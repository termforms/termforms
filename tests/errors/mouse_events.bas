' Errors test: the mouse events take four ByVal Integers (Button, Shift,
' X, Y); a Timer and a Menu have none.
Begin Form f
    Begin CommandButton cmdA
    End
    Begin Timer tmrA
    End
    Begin Menu mnuFile
        Caption = "&File"
    End
End

Sub cmdA_MouseDown(Button As Integer, Shift As Integer, X As Integer)
End Sub

Sub cmdA_MouseUp(ByRef Button As Integer, Shift As Integer, X As Integer, Y As Integer)
End Sub

Sub tmrA_MouseDown(Button As Integer, Shift As Integer, X As Integer, Y As Integer)
End Sub

Sub mnuFile_MouseMove(Button As Integer, Shift As Integer, X As Integer, Y As Integer)
End Sub
