' Run test: the file system: MkDir, Exists, FileCopy, FileLen, Name,
' FileDateTime (its form; the time itself varies), Dir$ with wildcards
' (sorted, files only) and Dir$() going on, Kill with a wildcard, ChDir
' and CurDir$, RmDir, and errors 53, 58, 75 and 76, caught. The program
' runs in a folder of its own (the test runner's).
Begin Form f
    Caption = "File system"
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

Sub Touch(path As String, text As String)
    Open path For Output As #1
    Print #1, text;
    Close #1
End Sub

Sub Form_Load()
    Dim s As String, name As String, stamp As String
    MkDir "box"
    Touch "box/b.txt", "hello"
    Touch "box/a.txt", "hi"
    Touch "box/c.tmp", ""
    Touch "box/d.tmp", ""
    MkDir "box/inner"
    FileCopy "box/b.txt", "box/copy.txt"
    Name "box/copy.txt" As "box/renamed.txt"
    stamp = FileDateTime("box/b.txt")
    t1.Text = Exists("box") & Exists("box/b.txt") & Exists("nope") & " " & _
        FileLen("box/renamed.txt") & " " & Len(stamp) & Mid$(stamp, 5, 1) & Mid$(stamp, 14, 1)

    name = Dir$("box/*.txt")
    Do While name <> ""
        s = s & name & ","
        name = Dir$()
    Loop
    s = s & " " & Dir$("box/?.tmp") & Dir$
    t2.Text = s

    Kill "box/*.tmp"
    ChDir "box"
    s = Right$(CurDir$, 3) & " " & Dir("*") & Dir() & Dir() & "|" & Dir()
    ChDir ".."
    t3.Text = s

    s = ""
    Try
        Kill "box/*.tmp"
    Catch When Err.Number = 53
        s = "53"
    End Try
    Try
        Name "box/a.txt" As "box/b.txt"
    Catch
        s = s & " " & Err.Number
    End Try
    Try
        RmDir "box"
    Catch
        s = s & " " & Err.Number
    End Try
    Try
        ChDir "no-such-folder"
    Catch
        s = s & " " & Err.Number
    End Try
    Try
        MkDir "box"
    Catch
        s = s & " " & Err.Number
    End Try
    Kill "box/*"
    RmDir "box/inner"
    RmDir "box"
    t4.Text = s & " " & Exists("box")
End Sub
