' Run test: Double division by zero is a runtime error (no Infinity).
Begin Form f
    Caption = "Divide"
    Begin CommandButton b
        Caption = "Go"
    End
End

Sub b_Click()
    Dim d As Double
    d = 1.5 / d
End Sub
