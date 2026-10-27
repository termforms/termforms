' Run test: an error no Catch matches leaves the event handler and is
' reported with its original line.
Begin Form f
    Caption = "Unmatched"
    Begin CommandButton b
        Caption = "Go"
    End
End

Sub Fail()
    Throw 1234, "nobody wants this"
End Sub

Sub b_Click()
    Try
        Fail
    Catch When Err.Number = 1
    End Try
End Sub
