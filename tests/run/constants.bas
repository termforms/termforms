' Run test: Const values (typed by value, built from other Consts and
' operators, worked out at compile time), Enum members and their numbering,
' Enum variables and Functions, CType, For over an Enum, Consts in a Sub.
Begin Form f
    Caption = "Constants"
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
End

Const BASE = 40
Const ANSWER = BASE + 2
Const BIGGER = 3000000000          ' too big for an Integer, so a Long
Const THIRD = 1 / 3
Const GREETING = "Answer=" & ANSWER & " third=" & THIRD
Const ENABLED = ANSWER = 42 And Not False
Const EXPONENTS = "" & 1.5E-7 & " " & 1E+20     ' Double text with exponents, made when compiling

Enum Weekday
    Monday = 1
    Tuesday
    Wednesday
    Saturday = 6
    Sunday
End Enum

Enum Code As Long
    Low = -5
    High = 9000000000
    Higher
End Enum

Function After(d As Weekday) As Weekday
    If d = Weekday.Sunday Then
        Return Weekday.Monday
    End If
    Return CType(d + 1, Weekday)
End Function

Sub Form_Load()
    Const SEP = ","
    Dim d As Weekday, s As String, n As Long

    t1.Text = GREETING & " " & ENABLED

    t2.Text = BIGGER * 2 & " " & Weekday.Monday & SEP & Weekday.Wednesday & SEP & _
              Weekday.Saturday & SEP & Weekday.Sunday & " " & Code.Low & SEP & Code.Higher

    d = Weekday.Saturday
    d = After(d)
    s = d & " then " & After(d)
    t3.Text = s

    For d = Weekday.Monday To Weekday.Sunday Step 2
        s = s & SEP & d
    Next
    t4.Text = s & " d=" & d

    n = Code.High - 1
    t5.Text = n & " " & CType(n Mod 10, Code) & " " & CType("3", Weekday) & " " & -Weekday.Tuesday & _
              " " & EXPONENTS
End Sub
