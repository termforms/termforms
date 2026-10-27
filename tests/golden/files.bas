' Golden test: Open in each text mode, Print # (items joined, a trailing
' ;), Line Input into a String, EOF, LOF, FreeFile and Close of one file
' and of all.
Begin Form frmMain
    Caption = "Files"
    Begin TextBox txtOut
        Width = 30
    End
End

Sub Form_Load()
    Dim n As Integer, line As String
    n = FreeFile
    Open "out.txt" For Output As #n
    Print #n, "n="; n;
    Print #n,
    Close #n
    Open "out.txt" For Input As 1
    Do While Not EOF(1)
        Line Input #1, line
    Loop
    txtOut.Text = line & LOF(1)
    Close
End Sub
