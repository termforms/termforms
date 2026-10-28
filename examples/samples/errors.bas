' errors.bas -- errors: Try, Catch (with When to choose), Finally (which
' always runs), Throw for your own, and Err's Number, Description, Line.
'
' Build and run:   tfc errors.bas && ./errors
Begin Form frmSample
    Caption = "Errors"
    Width = 70
    Height = 20
    Begin ListBox lstOut
        Left = 1
        Width = 66
        Height = 17
    End
End

' Each line of the sample's results.
Sub Note(text As String)
    lstOut.AddItem text
End Sub

Function Divide(a As Integer, b As Integer) As Integer
    Return a \ b
End Function

Function CheckAge(age As Integer) As Integer
    If age < 0 Or age > 150 Then
        Throw 1000, "an age must be 0 to 150, not " & age
    End If
    Return age
End Function

Sub Attempt(what As String, a As Integer, b As Integer)
    Try
        Note what & " = " & Divide(a, b)
    Catch When Err.Number = 11
        Note what & ": division by zero (error 11)"
    Catch
        Note what & ": error " & Err.Number & ", " & Err.Description
    Finally
        Note "  (Finally ran for " & what & ")"
    End Try
End Sub

Sub Form_Load()
    Attempt "10 \ 2", 10, 2
    Attempt "10 \ 0", 10, 0
    Dim big As Integer = 2147483647
    Try
        big = big + 1
    Catch
        Note "2147483647 + 1: error " & Err.Number & " (" & Err.Description & ") at line " & Err.Line
    End Try
    For Each age As String In Split("42 200")
        Try
            Note "age " & CheckAge(CInt(Val(age))) & " is fine"
        Catch When Err.Number >= 1000
            Note "your own error " & Err.Number & ": " & Err.Description
        End Try
    Next
End Sub
