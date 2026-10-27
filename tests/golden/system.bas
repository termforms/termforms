' Golden test: Command$ (main then takes argc and argv), Environ$, Shell
' as a function and as a statement, Now, Date$, Time$ and Timer.
Begin Form frmMain
    Caption = "System"
    Begin TextBox txtOut
        Width = 30
    End
End

Sub Form_Load()
    Dim code As Integer, t As Double
    code = Shell("exit 1")
    Shell "echo"
    t = Timer
    txtOut.Text = Command$ & Environ$("HOME") & code & Now & Date$ & Time$ & t
End Sub
