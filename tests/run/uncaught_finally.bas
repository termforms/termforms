' Run test: an error passing through a Finally with no Catch runs the
' Finally, then ends the program, reporting the line where it happened.
Begin Form f
    Caption = "Finally"
    Begin TextBox t
        Width = 20
    End
    Begin CommandButton b
        Caption = "Go"
        Top = 2
    End
End

Sub b_Click()
    Dim n As Integer
    Try
        n = 1 \ n
    Finally
        t.Text = "finally ran"
    End Try
End Sub
