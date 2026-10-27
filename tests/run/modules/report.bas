' Module of the modules run test: a Function also named in stock (so
' qualified where used), one only here, and a Sub that sets a control of
' the form, named through the form.

Function Label() As String
    Return "report"
End Function

Function Twice(n As Integer) As Integer
    Return n * 2
End Function

Sub Show(text As String)
    frmMain.t3.Text = text
End Sub
