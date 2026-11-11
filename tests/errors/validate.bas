' Errors test: Validate's Cancel is a ByRef Boolean (as VB6; Unload's is
' an Integer); SetFocus is for a control that can take the focus, not a
' Label.
Begin Form f
    Begin TextBox txtA
    End
    Begin Label lblA
    End
End

Sub txtA_Validate(ByRef Cancel As Integer)
End Sub

Sub Form_Load()
    lblA.SetFocus
    txtA.SetFocus
End Sub
