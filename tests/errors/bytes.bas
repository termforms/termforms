' Errors test: Byte under Option Strict: a constant that does not fit, an
' Integer variable, Byte arithmetic (an Integer) stored back, a negative
' constant, and CByte of a Boolean.
Begin Form f
    Begin CommandButton b
    End
End

Sub b_Click()
    Dim x As Byte, n As Integer
    x = 256
    x = n
    x = x + 1
    x = -1
    x = CByte(True)
End Sub
