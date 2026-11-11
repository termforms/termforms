' designer.bas -- tfdesign, the TermForms visual form designer: lays out
' a form's controls in the terminal and writes its form block (to a .frm
' beside the .bas, or in place in the .bas), and its event handlers'
' first lines into the .bas.
'
'   tfdesign main.bas        (or main.frm; a new name starts a new form)

Sub Main()
    Dim args() As String = CommandArgs()
    If UBound(args) <> 0 Then
        Print #0, "Usage: tfdesign <form.bas or form.frm>"
        Print #0, "Lays out the form in the terminal; saves its block to the .frm (or in place"
        Print #0, "in the .bas it is already in) and writes event handlers into the .bas."
        End 2
    End If
    NameFiles args(0)
    Dim problem As String = LoadCatalog()
    If problem = "" Then problem = LoadForm()
    If problem <> "" Then
        Print #0, "tfdesign: " & problem
        End 1
    End If
    frmDesigner.Show
End Sub
