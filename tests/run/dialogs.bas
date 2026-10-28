' Run test: MsgBox and InputBox over the form, answered by the key script:
' in Form_Load (Yes/No/Cancel, Alt+N), an InputBox edited and accepted, one
' cancelled (Esc gives ""), MsgBox as a statement, vbDefaultButton2 taking
' Enter, a Yes/No box that Esc does not close (Alt+Y does); then the
' script ends while a two-line information box is shown, so the screen
' shows it over the form. VB's named constants throughout. The form's
' Activate fires once: dialogs closing do not activate it again (VB6).
Begin Form frmMain
    Caption = "Dialogs"
    Width = 50
    Height = 18
    Begin CommandButton cmdAsk
        Caption = "Ask"
        Width = 10
    End
    Begin CommandButton cmdLast
        Caption = "Last"
        Left = 12
        Width = 10
    End
    Begin Label lblLog
        Top = 14
        Width = 48
        Height = 2
    End
End

Sub Note(s As String)
    lblLog.Caption = lblLog.Caption & s & " "
End Sub

Sub Form_Activate()
    Note "act"
End Sub

Sub Form_Load()
    Note "load:" & MsgBox("Save changes?", vbYesNoCancel + vbQuestion, "Editor")
End Sub

Sub cmdAsk_Click()
    Dim a As String, b As String
    a = InputBox("Your name:", "Hello", "Ann")
    b = InputBox("Again:")
    MsgBox "Hi " & a
    Note "[" & a & "][" & b & "] " & MsgBox("Delete?", vbOKCancel + vbDefaultButton2) & _
        " " & MsgBox("Sure?", vbYesNo + vbExclamation) & " " & (vbYes = 6)
End Sub

Sub cmdLast_Click()
    MsgBox "Line one" & vbCrLf & "Tom & Jerry", vbInformation
End Sub
