' Run test: the Mid$ statement replaces characters in place, counted in
' code points, and never changes the string's length: a length shorter
' than the text, the text longer than the room left, Mid without $, an
' array element and a Type field, a wide character replacing a narrow
' one; a start past the end is error 5, caught.
Begin Form frmMain
    Caption = "Mid$"
    Width = 58
    Height = 10
    Begin Label lblLog
        Width = 56
        Height = 8
    End
End

Type Person
    Name As String
End Type

Sub Note(s As String)
    lblLog.Caption = lblLog.Caption & s & " "
End Sub

Sub Form_Load()
    Dim s As String = "abc" & "defgh"
    Mid$(s, 2, 2) = "XYZ"
    Note s
    Mid$(s, 7) = "12345"
    Note s
    Mid(s, 1) = "q"
    Note s & " len=" & Len(s)

    Dim names(1) As String
    names(1) = "Jones"
    Mid$(names(1), 1, 1) = "B"
    Dim p As Person
    p.Name = "cat" & "s"
    Mid$(p.Name, 4) = "!"
    Note names(1) & " " & p.Name

    Dim w As String = "a" & "bcd"
    Mid$(w, 2, 1) = "中"
    Note w & " len=" & Len(w) & " lenb=" & LenB(w)

    Try
        Mid$(w, 9) = "x"
    Catch
        Note "E" & Err.Number
    End Try
End Sub
