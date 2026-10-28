' Golden test: Select Case lowered to C if/else (never a C switch). Cases
' of constants make one if/else chain, Or joining several tests; a Case
' with a Function call sets a hidden flag test by test, so later tests are
' not run once one matches; a String selector is held once and released.
Begin Form frmMain
    Caption = "Select"
End

Function Limit() As Integer
    Limit = 10
End Function

Sub Form_Load()
    Dim n As Integer
    Dim s As String
    n = 4
    Select Case n
        Case 1, 3 To 5
            n = 0
        Case Is > Limit(), 7
            n = 1
        Case Else
            n = 2
    End Select
    Select Case s & "x"
        Case "ax"
            s = ""
    End Select
End Sub
