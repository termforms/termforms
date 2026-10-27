' Golden test: the file-system statements as calls (Kill, MkDir, RmDir,
' ChDir, FileCopy, Name ... As) and functions (Dir$ and Dir$(), CurDir$,
' FileLen, FileDateTime, Exists).
Begin Form frmMain
    Caption = "Files"
    Begin TextBox txtOut
        Width = 30
    End
End

Sub Form_Load()
    Dim s As String
    MkDir "work"
    FileCopy "a.txt", "work/a.txt"
    Name "work/a.txt" As "work/b.txt"
    s = Dir$("work/*.txt") & Dir$()
    If Exists("work/b.txt") Then s = s & FileLen("work/b.txt") & FileDateTime("work/b.txt")
    ChDir "work"
    s = s & CurDir$
    ChDir ".."
    Kill "work/*"
    RmDir "work"
    txtOut.Text = s
End Sub
