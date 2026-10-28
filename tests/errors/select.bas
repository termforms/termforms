' Errors test: Select Case. A selector that is not a number, String,
' Boolean or Enum; a Case of the wrong type (as = would reject it); and
' the warnings: Cases an earlier one covers (a value, a range covered by
' two joined, an Is, a repeated String, not one differing in case) and
' an empty range. (The syntax errors are in select_syntax.bas.)
Type Point
    X As Integer
End Type

Sub Main()
    Dim p As Point
    Dim n As Integer
    Dim s As String
    Select Case p
        Case 1
    End Select
    Select Case n
        Case "one"
    End Select
    Select Case n
        Case 1 To 3
        Case 4 To 6
        Case 2 To 5
        Case 5
        Case Is >= 10
        Case 12
        Case 9 To 1
        Case 7, 8
    End Select
    Select Case s
        Case "a", "b"
        Case "b"
        Case "B"
    End Select
End Sub
