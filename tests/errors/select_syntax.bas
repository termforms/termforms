' Errors test: Select Case syntax: a comparison without Is, Case Else not
' last, a statement before the first Case, and a missing End Select.
Sub Main()
    Dim n As Integer
    Select Case n
        Case > 5
    End Select
    Select Case n
        Case Else
        Case 2
    End Select
    Select Case n
        n = 1
        Case 1
    End Select
    Select Case n
        Case 1
End Sub
