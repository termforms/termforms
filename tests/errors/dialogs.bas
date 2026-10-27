' Errors test: dialogs and VB's constants: a named constant assigned to or
' declared, MsgBox given a String for its buttons, and InputBox used as a
' statement (its answer is the point).
Begin Form f
    Begin CommandButton b
    End
End

Sub b_Click()
    Dim vbOK As Integer
    vbYes = 1
    MsgBox "x", "buttons"
    InputBox "name?"
End Sub
