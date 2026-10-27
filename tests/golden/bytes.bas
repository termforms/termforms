' Golden test: Byte variables, arrays and Type members (uint8_t), a
' constant stored directly, CByte from Integer, Long and Double, Byte
' arithmetic done as Integer, and a For loop over a Byte.
Begin Form frmMain
    Caption = "Bytes"
    Begin TextBox txtOut
        Width = 30
    End
End

Type Pixel
    R As Byte
End Type

Sub Form_Load()
    Dim b As Byte, buf(3) As Byte, p As Pixel, n As Integer, big As Long
    b = 65
    buf(1) = CByte(n)
    p.R = CByte(CByte(big) + CByte(1.5))
    n = b * 2 + buf(1)
    For b = 1 To 3
        n = n + b
    Next
    txtOut.Text = b & n
End Sub
