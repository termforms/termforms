' Errors test: rules for Try/Catch/Finally, Throw, Err.Raise and Err.
Begin Form f
    Begin CommandButton b
    End
End

Sub Main2()
    Dim i As Integer, s As String
    Throw
    Throw "x"
    Throw 1, 2
    Throw 1, "a", "b"
    Err.Raise
    Err.Raise 1, 2
    Err.Number = 5
    i = Err.Count
    i = Err
    Try
        i = 1
    Catch When i
    Finally
        Exit Sub
    End Try
    For i = 1 To 2
        Try
        Finally
            Exit For
        End Try
    Next
    Try
    Finally
        For i = 1 To 2
            Exit For
        Next
        Return
    End Try
End Sub

Sub Uses()
    Dim Err As Integer
End Sub
