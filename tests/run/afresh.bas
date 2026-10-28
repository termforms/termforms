' Run test: a form unloaded is made afresh when it is next shown, as VB:
' frmAsk (see afresh.with) is edited, then unloads itself; the main form
' can still read what was entered (the form is remade at its next Load,
' not at Unload); shown again, its TextBox, CheckBox and Caption are back
' to their form-block settings and its ListBox is empty before Form_Load
' fills it again (one item, not two).
Begin Form frmMain
    Caption = "Main"
    Width = 50
    Height = 10
    Begin CommandButton cmdAsk
        Caption = "Ask"
        Width = 10
    End
    Begin Label lblLog
        Top = 2
        Width = 48
        Height = 5
    End
End

Sub Note(s As String)
    lblLog.Caption = lblLog.Caption & s & " "
End Sub

Sub cmdAsk_Click()
    frmAsk.Show vbModal
    Note "[" & frmAsk.txtA.Text & "] chk=" & frmAsk.chkA.Value & " n=" & frmAsk.lstA.ListCount & " " & frmAsk.Caption
End Sub
