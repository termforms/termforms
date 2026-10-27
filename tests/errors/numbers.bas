' Errors test: Option Strict with Long and Double, the built-in functions'
' argument rules, and their reserved names.
Begin Form f
    Begin CommandButton b
    End
End

Sub Val()
End Sub

Sub TakesLong(ByRef n As Long, i As Integer)
End Sub

Sub Main2()
    Dim i As Integer, n As Long, d As Double, s As String, ok As Boolean
    Dim CInt As Integer
    i = 1.5
    i = n
    n = d
    d = "1"
    s = d
    d = 5 \ 2.5
    i = CInt(True)
    d = Val(5)
    s = Str$("x")
    i = CInt(1, 2)
    d = Val
    CStr 5
    i = cint(5)
    i = 2147483648%
    For s = "a" To "b"
    Next
    For i = 1 To 10 Step 0.5
    Next
    TakesLong i, n
    ok = d > s
End Sub
