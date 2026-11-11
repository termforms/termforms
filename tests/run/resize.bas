' Run test: Screen.Width, Screen.Height and Form_Resize. Form_Load can
' already read the terminal's size (before the form is shown); Resize
' comes after Load and before Activate, then at each change of the
' terminal's size ({RESIZE}), and the form fits itself to the terminal
' each time, moving its button to the bottom-right corner.
Begin Form frmMain
    Caption = "Resize"
    Width = 30
    Height = 8
    Begin Label lblLog
        Width = 28
        Height = 4
    End
    Begin CommandButton cmdClose
        Caption = "Close"
        Width = 9
    End
End

Sub Note(s As String)
    lblLog.Caption = lblLog.Caption & s & " "
End Sub

Sub Form_Load()
    Note "Load " & Screen.Width & "x" & Screen.Height
End Sub

Sub Form_Activate()
    Note "Activate"
End Sub

Sub Form_Resize()
    Me.Width = Screen.Width
    Me.Height = Screen.Height
    lblLog.Width = Me.Width - 2
    cmdClose.Left = Me.Width - 2 - cmdClose.Width
    cmdClose.Top = Me.Height - 3
    Note "Resize " & Me.Width & "x" & Me.Height
End Sub
