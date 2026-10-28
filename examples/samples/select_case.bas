' select_case.bas -- Select Case: values, ranges, Is comparisons, several
' tests to a Case, Case Else, and Strings (exact, case matters).
'
' Build and run:   tfc select_case.bas && ./select_case
Begin Form frmSample
    Caption = "Select Case"
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

Function Grade(score As Integer) As String
    Select Case score
        Case 100
            Return "perfect"
        Case 90 To 99
            Return "A"
        Case 75 To 89, 70
            Return "B"
        Case Is >= 50
            Return "pass"
        Case Is < 0
            Return "not a score"
        Case Else
            Return "fail"
    End Select
End Function

Sub Form_Load()
    Dim scores() As Integer = {100, 95, 80, 70, 72, 55, 10, -5}
    For Each score As Integer In scores
        Note score & " is " & Grade(score)
    Next
    Dim words() As String = {"yes", "Yes", "no", "maybe"}
    For Each word As String In words
        Select Case word
            Case "yes", "y"
                Note word & ": agreed"
            Case "no", "n"
                Note word & ": declined"
            Case Else
                Note word & ": not understood (Strings compare exactly)"
        End Select
    Next
End Sub
