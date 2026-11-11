' Errors test: MultiLine and ScrollBars are set in the form block only (as
' VB), and ScrollBars is 0 to 3.
Begin Form f
    Begin TextBox txtNotes
        MultiLine = True
        ScrollBars = 4
    End
End

Sub Form_Load()
    txtNotes.MultiLine = False
    txtNotes.ScrollBars = 2
    If txtNotes.MultiLine And txtNotes.ScrollBars = 0 Then
        txtNotes.Text = "read is fine"
    End If
End Sub
