' Run test: Validate, CausesValidation and SetFocus. Form_Load puts the
' focus in cboCity with SetFocus (SetFocus on a hidden control is error
' 5). cboCity's Validate refuses an empty city and takes a typed one,
' adding it to the list if it is new (so a city typed twice is added
' once). txtAge's Validate refuses anything not a number, selecting it:
' Tab, Enter (the Default button) and a click on the button are all
' refused while it is wrong; Esc still presses the Cancel button, whose
' CausesValidation is False. Once the age is right, Enter presses OK.
Begin Form frmMain
    Caption = "Validate"
    Width = 50
    Height = 12
    Begin TextBox txtAge
        Width = 8
    End
    Begin ComboBox cboCity
        Left = 10
        Width = 16
    End
    Begin CommandButton cmdOK
        Caption = "OK"
        Left = 28
        Width = 8
        Default = True
    End
    Begin CommandButton cmdCancel
        Caption = "Cancel"
        Left = 38
        Width = 10
        Cancel = True
        CausesValidation = False
    End
    Begin TextBox txtHidden
        Top = 1
        Visible = False
    End
    Begin Label lblLog
        Top = 2
        Width = 48
        Height = 8
    End
End

Sub Note(s As String)
    lblLog.Caption = lblLog.Caption & s & " | "
End Sub

Sub Form_Load()
    cboCity.AddItem "Athens"
    cboCity.AddItem "Melbourne"
    Try
        txtHidden.SetFocus
    Catch
        Note "E" & Err.Number
    End Try
    cboCity.SetFocus
    Note "cv=" & cmdOK.CausesValidation & "/" & cmdCancel.CausesValidation
End Sub

Sub cboCity_GotFocus()
    Note "city focus"
End Sub

Sub cboCity_Validate(ByRef Cancel As Boolean)
    If cboCity.Text = "" Then
        Note "empty city"
        Cancel = True
    ElseIf cboCity.ListIndex = -1 Then
        For i As Integer = 0 To cboCity.ListCount - 1
            If cboCity.List(i) = cboCity.Text Then
                cboCity.ListIndex = i
                Note "found " & cboCity.Text
                Exit Sub
            End If
        Next
        cboCity.AddItem cboCity.Text
        Note "added " & cboCity.Text & " n=" & cboCity.ListCount
    End If
End Sub

Sub txtAge_Validate(ByRef Cancel As Boolean)
    If CStr(CInt(Val(txtAge.Text))) <> txtAge.Text Then
        Note "bad age " & txtAge.Text
        txtAge.SelStart = 0
        txtAge.SelLength = Len(txtAge.Text)
        Cancel = True
    End If
End Sub

Sub cmdOK_Click()
    Note "OK " & txtAge.Text & " " & cboCity.Text
End Sub

Sub cmdCancel_Click()
    Note "cancel"
End Sub
