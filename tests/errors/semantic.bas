' Errors test: several independent semantic errors in one build.
Begin Form f
    Caption = 1
    Begin TextBox t
        Colour = 4
    End
    Begin Grid l
    End
End

Sub T_Change()
End Sub

Sub t_change()
End Sub

Sub Main2()
    Dim s As String
    s = 42
    undeclared = 1
    t.Txt = "x"
    If s Then s = "y"
    Main
End Sub
