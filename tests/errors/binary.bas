' Errors test: binary file statements: a buffer that is not a Byte array
' (a String array, a single Byte), a Get with a count into a fixed array,
' Get into a ByVal array, and an offset that is not a whole number.
Begin Form f
    Begin CommandButton b
    End
End

Sub Fill(buf() As Byte)
    Get #1, 0, buf
End Sub

Sub b_Click()
    Dim names(2) As String, one As Byte, fixed(3) As Byte, buf() As Byte
    Get #1, 0, names
    Put #1, 0, one
    Get #1, 0, fixed, 2
    Get #1, 1.5, buf
End Sub
