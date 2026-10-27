' Run test: text files: Open For Output, Append and Input; Print # with
' ; joining (no added spaces) and a trailing ; leaving out the newline;
' Line Input and EOF; a CRLF line read without its CR; LOF, Seek and
' FreeFile; Close of several numbers, and of every file; and the errors
' 53, 54, 55, 52, 62 and 76, caught. The program runs in a folder of its
' own (the test runner's), so it writes there.
Begin Form f
    Caption = "Files"
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
    Dim n As Integer, line As String, s As String, count As Integer, size As Long
    n = FreeFile
    Open "notes.txt" For Output As #n
    Print #n, "one"
    Print #n, "n="; 5; " pi="; 3.5; " ok="; True
    Print #n, "no newline: ";
    Print #n, "joined"
    Print #n, "crlf" & Chr$(13)
    Print #n,
    Close #n

    Open "notes.txt" For Append As 2
    Print #2, "€ appended"
    Close

    Open "notes.txt" For Input As #1
    size = LOF(1)
    Do While Not EOF(1)
        Line Input #1, line
        count = count + 1
        s = s & "[" & line & "]"
    Loop
    t1.Text = count & " " & size & " " & Seek(1) & " " & FreeFile
    Close #1, #2
    t2.Text = Mid$(s, 1, 45)
    t3.Text = Mid$(s, 46)

    s = ""
    Try
        Open "missing.txt" For Input As #1
    Catch When Err.Number = 53
        s = "53"
    End Try
    Open "notes.txt" For Input As #1
    Try
        Print #1, "x"
    Catch
        s = s & " " & Err.Number
    End Try
    Try
        Open "other.txt" For Output As #1
    Catch
        s = s & " " & Err.Number
    End Try
    Try
        Line Input #0, line
    Catch
        s = s & " " & Err.Number
    End Try
    Do While Not EOF(1)
        Line Input #1, line
    Loop
    Try
        Line Input #1, line
    Catch
        s = s & " " & Err.Number
    End Try
    Close #1
    Try
        Open "no-such-folder/x.txt" For Output As #1
    Catch
        s = s & " " & Err.Number
    End Try
    t4.Text = s
End Sub
