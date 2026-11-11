' Run test: the form block in form_file.frm, beside this file (as the
' designer writes it); the code here wires to its controls as usual.
Sub cmdOK_Click()
    lblNote.Caption = "clicked " & Me.Caption
End Sub
