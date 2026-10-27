' Golden test: Try/Catch When/Finally, Throw and Err.Raise, rethrow,
' Exit and Return through Finally, errors propagating from a Function.
Begin Form frmMain
    Caption = "Errors"
    Begin TextBox txtOut
        Width = 30
    End
End

Dim log As String

Function Ratio(a As Integer, b As Integer) As Integer
    Ratio = a \ b
End Function

Sub Form_Load()
    Dim i As Integer, r As Integer
    Try
        r = Ratio(1, 0)
        log = "not reached"
    Catch When Err.Number = 11
        log = Err.Description & " at " & Err.Line
    Catch
        Throw
    Finally
        log = log & "."
    End Try
    For i = 1 To 3
        Try
            If i = 2 Then Exit For
            Err.Raise 1000 + i, "Form_Load", "custom"
        Catch
            log = log & Err.Number
        End Try
    Next
    txtOut.Text = log & Twice(i)
End Sub

Function Twice(n As Integer) As Integer
    Try
        Return n * 2
    Finally
        log = log & "!"
    End Try
End Function
