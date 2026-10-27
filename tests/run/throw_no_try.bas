' Run test: Throw in a program with no Try ends it at once with the message.
Begin Form f
    Caption = "Throw"
    Begin CommandButton b
        Caption = "Go"
    End
End

Sub b_Click()
    Throw 5000, "custom failure"
End Sub
