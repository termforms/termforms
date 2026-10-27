' Errors test: file statements the parser rejects: a mode Open does not
' have, and Print # items separated by a comma.
Sub Main()
    Open "a.txt" For Random As #1
    Print #1, "a", "b"
End Sub
