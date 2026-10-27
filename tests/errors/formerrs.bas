' Errors test: forms and events: a handler whose argument is meant to be
' changed but is ByVal (the change would be lost), a KeyDown with too few
' parameters, Show on something that is not a form, Me in a module with no
' form (see formerrs.with), and two forms of the same name.
Begin Form frmMain
    Begin TextBox txt
    End
End

Sub Form_Unload(Cancel As Integer)
    Cancel = 1
End Sub

Sub txt_KeyDown(ByRef KeyCode As Integer)
End Sub

Sub Form_Load()
    txt.Show
End Sub
