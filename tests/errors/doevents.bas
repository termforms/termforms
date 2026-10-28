' Errors test: DoEvents is a statement only (VB6's return value is not
' kept), and a form's Visible can be read but not set (Show and Hide set
' it).
Begin Form frmMain
    Caption = "x"
End

Sub Form_Load()
    Dim n As Integer
    n = DoEvents()
    Me.Visible = False
End Sub
