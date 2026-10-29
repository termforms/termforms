' Run test: Print while a form is on screen is held and written after the
' terminal is restored (here, after the headless screen dump), in order,
' Print #0 too; End n out of range is error 5, catchable; and Line Input
' while a form is shown is error 900 (console_forms.stderr), as the keys
' belong to the form.
Begin Form f
    Caption = "Held"
    Width = 30
    Height = 6
    Begin CommandButton cmdGo
        Caption = "&Go"
        Width = 10
    End
    Begin CommandButton cmdRead
        Caption = "&Read"
        Top = 2
        Width = 10
    End
End

Sub cmdGo_Click()
    Print "clicked"
    Print #0, "to standard error"
    Try
        End 300
    Catch
        Print "End 300: "; Err.Number
    End Try
End Sub

Sub cmdRead_Click()
    Dim s As String
    Line Input s
End Sub
