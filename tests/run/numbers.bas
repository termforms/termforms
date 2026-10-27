' Run test: Long and Double results, text of numbers, rounding, Str$ and
' Val, operator precedence, and For loops over Double and Long.
Begin Form f
    Caption = "Numbers"
    Width = 56
    Height = 10
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
    Begin TextBox t4
        Top = 3
        Width = 50
    End
    Begin TextBox t5
        Top = 4
        Width = 50
    End
    Begin TextBox t6
        Top = 5
        Width = 50
    End
End

Sub Form_Load()
    Dim n As Long, d As Double, i As Integer, count As Integer

    n = 2147483647
    n = n + 1
    t1.Text = n & " " & n * 1000 & " " & &HFF & " " & 7 Mod -3 & " " & -7 Mod 3 & " " & 7 \ 2

    t2.Text = (0.1 + 0.2) & " " & 1 / 3

    t3.Text = CInt(2.5) & " " & CInt(3.5) & " " & CInt(-2.5) & " " & CInt(2.6) & " " & _
              Fix(-2.7) & " " & Int(-2.7) & " " & CLng(1E15)

    t4.Text = "[" & Str$(5) & "][" & Str(-5) & "][" & CStr(5) & "][" & Str$(0.25) & "] " & _
              10 / 4 & " " & 2 ^ 10 & " " & 1E20 & " " & 1.5E-7

    t5.Text = Val(" 1 2 3abc") + Val("&HFF") & " " & CDbl("1.5e3") & " " & _
              CLng("9223372036854775807") & " " & CInt("  42 ")

    For d = 0 To 1 Step 0.25
        count = count + 1
    Next
    i = 0
    For n = 3000000000 To 1 Step -1000000000
        i = i + 1
    Next
    t6.Text = -2 ^ 2 & " " & 2 ^ 3 ^ 2 & " " & 2 ^ -1 & " " & 10 - 3 Mod 2 & " " & _
              count & " " & i & " " & n
End Sub
