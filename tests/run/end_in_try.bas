' Run test: End inside a Try ends the program at once; the Finally does not
' run (settled 27 Oct 2026, the one exception to "Finally always runs").
Begin Form f
    Caption = "End"
    Begin TextBox t
        Width = 20
    End
    Begin CommandButton b
        Caption = "Go"
        Top = 2
    End
End

Sub b_Click()
    Try
        t.Text = "before End"
        End
    Finally
        t.Text = "Finally ran"
    End Try
End Sub
