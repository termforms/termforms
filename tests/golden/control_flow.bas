' Golden test: If/ElseIf/Else, Integer arithmetic, Boolean logic,
' string comparison with temporaries, module and local variables, calls.
Begin Form frmMain
    Caption = "Control flow"
    Begin TextBox txtOut
        Width = 30
    End
    Begin CommandButton cmdGo
        Caption = "Go"
        Top = 2
    End
End

Dim total As Integer
Dim label$

Sub Form_Load()
    label = "start"
    Reset
End Sub

Sub Reset()
    total = 0
End Sub

Sub cmdGo_Click()
    Dim n%, done As Boolean
    n = (total + 2) * 3 \ 2 - -1
    done = n >= 10 Or Not (total < 5)
    If txtOut.Text & "!" = "stop!" Then
        End
    ElseIf done And n <> 0 Then
        txtOut.Text = label & " " & n & " " & done
    Else
        total = total + n
        Call Reset
    End If
End Sub
