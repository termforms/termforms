' Run test: a Timer has no place on the form, so a click on a control at
' the form's top left reaches it, though the Timer comes after it (it
' used to take the click: fixed after 1.0.0).
Begin Form f
    Caption = "T"
    Width = 30
    Height = 6
    Begin CommandButton cmdA
        Caption = "A"
        Width = 8
    End
    Begin Label lbl
        Top = 2
        Width = 20
    End
    Begin Timer tmr
    End
End
Sub cmdA_Click()
    lbl.Caption = "clicked"
End Sub
