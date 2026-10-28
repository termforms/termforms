' Run test: Select Case. Numbers with values, ranges and Is, several tests
' to a Case, Case Else; a String (case matters, as =); an Enum; a Double
' selector against Integer Cases (widening); Cases that call a Function,
' tried in order and stopping at the first match (the calls are logged);
' the selector evaluated once; a Select with no Case matching and no Case
' Else; Exit For from inside a Case; a nested Select.
Begin Form frmMain
    Caption = "Select"
    Width = 58
    Height = 14
    Begin Label lblLog
        Width = 56
        Height = 12
    End
End

Enum Colour
    Red
    Green
    Blue
End Enum

Dim calls As String

Sub Note(s As String)
    lblLog.Caption = lblLog.Caption & s & " "
End Sub

Function Grade(score As Integer) As String
    Select Case score
        Case 100
            Grade = "perfect"
        Case 90 To 99, 85
            Grade = "A"
        Case Is < 0, Is > 100
            Grade = "bad"
        Case Is >= 50
            Grade = "pass"
        Case Else
            Grade = "fail"
    End Select
End Function

Function Limit(n As Integer) As Integer
    calls = calls & "L" & n
    Limit = n
End Function

Function NextValue() As Integer
    calls = calls & "N"
    NextValue = 7
End Function

Sub Form_Load()
    Note Grade(100) & Grade(95) & Grade(85) & Grade(86) & Grade(50) & Grade(-1) & Grade(101) & Grade(3)

    Dim s As String
    For Each s In Split("apple Apple pear kiwi")
        Select Case s
            Case "apple", "pear"
                Note s & ":fruit"
            Case Else
                Note s & ":other"
        End Select
    Next

    Dim c As Colour
    c = Colour.Blue
    Select Case c
        Case Colour.Red
            Note "red"
        Case Colour.Green, Colour.Blue
            Note "green or blue"
    End Select

    Dim d As Double
    d = 2.5
    Select Case d
        Case 1 To 2
            Note "low"
        Case 2 To 3
            Note "mid"
    End Select

    Select Case NextValue()
        Case Limit(1), Limit(7), Limit(9)
            Note "seven"
        Case Limit(20)
            Note "never"
    End Select
    Note calls

    Select Case 42
        Case 1
            Note "one"
    End Select

    Dim i As Integer
    For i = 1 To 10
        Select Case i
            Case 3
                Note "exit at " & i
                Exit For
        End Select
    Next
    Note "after " & i

    Select Case 2
        Case 2
            Select Case "x"
                Case "x"
                    Note "nested"
            End Select
    End Select
End Sub
