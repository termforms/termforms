' Run test: the Byte type: constants 0 to 255 stored directly, CByte
' (rounding half to even, from a String too), arithmetic done as Integer
' (b * 2 can exceed 255), widening into Integer and Long, Byte arrays and
' Type members, a For loop over a Byte with a negative Step, text with &,
' and error 6 when a value does not fit, caught.
Begin Form f
    Caption = "Bytes"
    Width = 56
    Height = 8
    Begin TextBox t1
        Width = 50
    End
    Begin TextBox t2
        Top = 1
        Width = 50
    End
    Begin TextBox t3
        Top = 2
        Width = 50
    End
End

Type Pixel
    R As Byte
    G As Byte
End Type

Sub Form_Load()
    Dim b As Byte, c As Byte, n As Integer, total As Long, buf(3) As Byte, p As Pixel, s As String
    b = 200
    c = CByte(CByte(2.5) + CByte("40")) ' 2 + 40, added as Integers
    n = b * 2                     ' 400: done as an Integer
    total = b
    buf(0) = 255
    buf(3) = CByte(buf(0) - 1)
    p.R = 7
    t1.Text = b & " " & n & " " & total & " " & buf(0) & buf(3) & " " & p.R & " " & (b > c)

    For b = 9 To 3 Step -3        ' 9, 6, 3; then 0 ends it (To 0 would step to -1: error 6)
        s = s & b
    Next
    t2.Text = s & " " & b

    s = ""
    Try
        b = CByte(256)
    Catch When Err.Number = 6
        s = "6"
    End Try
    Try
        b = CByte(-1)
    Catch
        s = s & " " & Err.Number
    End Try
    t3.Text = s & " " & b
End Sub
