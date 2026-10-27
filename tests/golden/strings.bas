' Golden test: string functions with their defaults filled in (Mid$'s
' length, InStr's start, Split's delimiter), owned results released, an
' Integer argument, and Split into an array then Join.
Begin Form frmMain
    Caption = "Strings"
    Begin TextBox txtOut
        Width = 30
    End
End

Sub Form_Load()
    Dim s As String, parts() As String, n As Integer
    s = Trim$("  a,b  ")
    n = InStr(s, ",") + InStrRev(s, "b")
    parts = Split(s)
    txtOut.Text = Mid$(s, n) & Left(UCase$(s), 1) & Join(parts, Chr$(45))
End Sub
