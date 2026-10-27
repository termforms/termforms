' Golden test: Long and Double variables and literals, widening, the
' operators / \ Mod ^, conversions and number functions, and For loops
' over Long and Double.
Begin Form frmMain
    Caption = "Numbers"
    Begin TextBox txtOut
        Width = 30
    End
End

Dim big&
Dim ratio As Double

Function Half(x As Double) As Double
    Half = x / 2
End Function

Sub Form_Load()
    Dim i As Integer, n As Long, d#, s As String
    n = 5000000000
    i = &HFF + &O17
    d = 1.5E-3 + 2#
    big = n * i + 7&
    ratio = Half(i) + d ^ 2 - -d
    n = n \ 3 + n Mod 7 + CLng(ratio)
    i = CInt(d) + CInt("12") + CInt(n Mod 100) + Fix(i)
    d = CDbl(i) + Val(s) + Int(-2.5) + Fix(ratio)
    If n > d And i <= ratio Then s = Str$(n) & Str(d) & CStr(i) & CStr(True)
    For d = 0.5 To 2 Step 0.5
        ratio = ratio + d
    Next
    For n = big To 1 Step -1000000000
        i = i + 1
    Next
    txtOut.Text = s & n & " " & ratio & " " & -n & CStr(1.0)
End Sub
