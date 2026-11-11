' Errors test: a FormView's statements are Clear, AddControl and
' SetProperty; its functions take the right arguments; a ListBox's
' methods are not a FormView's, nor a FormView's a ListBox's.
Begin Form f
    Begin FormView fvA
    End
    Begin ListBox lstA
    End
End

Sub Form_Load()
    Dim s As String
    fvA.AddItem "x"
    fvA.SetProperty "a", "b"
    s = fvA.ControlAt("1", 2)
    s = fvA.GetProperty("a")
    lstA.AddControl "Form", "x"
    fvA.Selected("a") = 3
    fvA.SetFocus
End Sub
