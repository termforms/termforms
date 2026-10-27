' Run test: Command$ (the test passes "alpha" and "beta gamma" as
' arguments; see system.args), Environ$ (PATH is set everywhere, the other
' variable nowhere), Shell as a function (its exit code) and as a
' statement (writing a file read back), the forms of Now, Date$, Time$ and
' Timer (their values vary), and error 5 for Shell "", caught.
Begin Form f
    Caption = "System"
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
End

Sub Form_Load()
    Dim s As String, code As Integer, line As String, t As Double, stamp As String
    t1.Text = "[" & Command$ & "] " & (Len(Environ$("PATH")) > 0) & " [" & _
        Environ$("TERMFORMS_TEST_NOT_SET") & "]"

    code = Shell("exit 3")
    Shell "echo hi> shell.txt"
    Open "shell.txt" For Input As #1
    Line Input #1, line
    Close #1
    t2.Text = code & " " & Trim$(line)

    stamp = Now
    t = Timer
    t3.Text = Len(stamp) & Mid$(stamp, 5, 1) & Mid$(stamp, 11, 1) & Mid$(stamp, 17, 1) & " " & _
        Len(Date$) & Mid$(Date, 3, 1) & " " & Len(Time$) & Mid$(Time, 6, 1) & " " & _
        (t >= 0 And t < 86400)

    Try
        Shell ""
    Catch When Err.Number = 5
        s = "5"
    End Try
    t4.Text = s
End Sub
