' Golden test: Open For Binary, Put and Get with and without an offset
' and a count (each given to the runtime with its flag), Seek # and Seek().
Begin Form frmMain
    Caption = "Binary"
    Begin TextBox txtOut
        Width = 30
    End
End

Sub Form_Load()
    Dim buf() As Byte, at As Long
    ReDim buf(15)
    Open "data.bin" For Binary As #1
    Put #1, , buf
    Seek #1, 4
    Get #1, , buf, 8
    at = 2
    Get #1, at, buf
    txtOut.Text = Seek(1) & " " & UBound(buf)
    Close #1
End Sub
