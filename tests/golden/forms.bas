' Golden test: two forms (see forms.with), each made with its controls,
' a control name both have (C names given the form), every form's events
' wired (Load as an event; Unload and KeyPress, with arguments, cast), and
' the first form started; Show vbModal, Hide, Load and Unload as calls.
Begin Form frmMain
    Caption = "Main"
    Begin CommandButton cmdOK
        Caption = "Go"
    End
End

Sub Form_Unload(ByRef Cancel As Integer)
    Cancel = 0
End Sub

Sub cmdOK_Click()
    Load frmOther
    frmOther.Show vbModal
    Unload Me
End Sub
