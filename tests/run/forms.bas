' Run test: two forms (see forms.with): frmOptions shown modal from a
' button and hidden by its OK (Default) button, the choice passed back in
' a Public variable; Load and Activate on each (Activate again when the
' main form is uncovered); GotFocus and LostFocus; KeyPress making letters
' upper case and throwing digits away; KeyDown seeing F2; and Unload Me,
' refused once by Form_Unload's Cancel, then allowed, which ends the
' program (the screen shows the form as it was last seen).
Begin Form frmMain
    Caption = "Main"
    Width = 50
    Height = 10
    Begin TextBox txtName
        Width = 12
    End
    Begin CommandButton cmdOptions
        Caption = "Options"
        Top = 1
        Width = 12
    End
    Begin CommandButton cmdClose
        Caption = "Close"
        Top = 2
        Width = 12
    End
    Begin Label lblLog
        Top = 4
        Width = 48
        Height = 4
    End
End

Dim tries As Integer

Sub Note(s As String)
    lblLog.Caption = lblLog.Caption & s & " "
End Sub

Sub Form_Load()
    Note "L"
End Sub

Sub Form_Activate()
    Note "A"
End Sub

Sub Form_Unload(ByRef Cancel As Integer)
    tries = tries + 1
    If tries = 1 Then
        Note "U?"
        Cancel = 1
    Else
        Note "U!"
    End If
End Sub

Sub txtName_GotFocus()
    Note "G"
End Sub

Sub txtName_LostFocus()
    Note "LF:" & txtName.Text
End Sub

Sub txtName_KeyPress(ByRef KeyAscii As Integer)
    If KeyAscii >= Asc("0") And KeyAscii <= Asc("9") Then
        KeyAscii = 0
    ElseIf KeyAscii >= Asc("a") And KeyAscii <= Asc("z") Then
        KeyAscii = KeyAscii - 32
    End If
End Sub

Sub cmdOptions_Click()
    frmOptions.Show vbModal
    Note "back:" & chosen
End Sub

Sub cmdClose_Click()
    Unload Me
End Sub
