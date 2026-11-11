' Run test: FormView. Form_Load describes a form in fvForm (a menu, a
' Label, a TextBox, a Frame holding an OptionButton, a hidden CheckBox
' that is drawn all the same, a button, a Timer that is not), selects the
' button and the form (each gets a handle), and tries the errors: 438 for
' an unknown type or property, 5 for a name used twice or a container
' that cannot hold the control, 380 for a value that does not convert or
' fit, 383 for a read-only property. GetProperty gives values as text,
' defaults included. Clicks then ask ControlAt and HandleAt; a
' double-click is DblClick; the last click turns on TabNumbers and
' scrolls the view one cell each way.
Begin Form frmMain
    Caption = "FormView"
    Width = 70
    Height = 22
    Begin FormView fvForm
        Left = 1
        Top = 1
        Width = 40
        Height = 13
    End
    Begin CommandButton cmdTab
        Caption = "Tab"
        Left = 1
        Top = 15
        Width = 7
    End
    Begin Label lblLog
        Left = 42
        Width = 26
        Height = 19
    End
End

Sub Note(s As String)
    lblLog.Caption = lblLog.Caption & s & " "
End Sub

Sub TryAdd(kind As String, name As String, container As String)
    Try
        fvForm.AddControl kind, name, container
    Catch
        Note "add " & name & ":" & Err.Number
    End Try
End Sub

Sub TrySet(name As String, prop As String, value As String)
    Try
        fvForm.SetProperty name, prop, value
    Catch
        Note prop & ":" & Err.Number
    End Try
End Sub

Sub Form_Load()
    ' A first form, cleared away (its controls freed) before the real one.
    fvForm.AddControl "Form", "frmJunk"
    fvForm.AddControl "ListBox", "lstJunk"
    fvForm.AddControl "TextBox", "txtJunk"
    fvForm.SetProperty "txtJunk", "Text", "gone" & "!"
    fvForm.Clear
    With fvForm
        .AddControl "Form", "frmG"
        .SetProperty "frmG", "Caption", "Guest"
        .SetProperty "frmG", "Width", "30"
        .SetProperty "frmG", "Height", "11"
        .AddControl "Menu", "mnuFile"
        .SetProperty "mnuFile", "Caption", "&File"
        .AddControl "Menu", "mnuOpen", "mnuFile"
        .SetProperty "mnuOpen", "Caption", "&Open"
        .AddControl "Label", "lblName"
        .SetProperty "lblName", "Caption", "&Name:"
        .SetProperty "lblName", "Left", "1"
        .SetProperty "lblName", "Top", "1"
        .AddControl "TextBox", "txtName"
        .SetProperty "txtName", "Left", "8"
        .SetProperty "txtName", "Top", "1"
        .SetProperty "txtName", "Width", "18"
        .AddControl "Frame", "fraKind"
        .SetProperty "fraKind", "Caption", "Kind"
        .SetProperty "fraKind", "Left", "1"
        .SetProperty "fraKind", "Top", "3"
        .SetProperty "fraKind", "Width", "14"
        .SetProperty "fraKind", "Height", "4"
        .AddControl "OptionButton", "optA", "fraKind"
        .SetProperty "optA", "Caption", "&A"
        .AddControl "CheckBox", "chkHidden"
        .SetProperty "chkHidden", "Caption", "Hidden"
        .SetProperty "chkHidden", "Left", "16"
        .SetProperty "chkHidden", "Top", "3"
        .SetProperty "chkHidden", "Visible", "false"
        .AddControl "CommandButton", "cmdOK"
        .SetProperty "cmdOK", "Caption", "OK"
        .SetProperty "cmdOK", "Left", "18"
        .SetProperty "cmdOK", "Top", "5"
        .SetProperty "cmdOK", "Width", "8"
        .SetProperty "cmdOK", "Default", "True"
        .AddControl "Timer", "tmrTick"
        .AddControl "ListBox", "lstHidden"
        .SetProperty "lstHidden", "Left", "40"
        .Selected("cmdOK") = True
        .Selected("frmG") = True
    End With
    TryAdd "Gizmo", "x", ""
    TryAdd "Label", "lblName", ""
    TryAdd "Label", "lblIn", "txtName"
    TryAdd "Menu", "mnuIn", "fraKind"
    TryAdd "Form", "frmOther", ""
    TrySet "cmdOK", "Colour", "red"
    TrySet "cmdOK", "Width", "wide"
    TrySet "txtName", "ScrollBars", "9"
    TrySet "cmdOK", "Default", "yes"
    TrySet "lstHidden", "ListCount", "3"
    TrySet "nobody", "Width", "3"
    Note fvForm.GetProperty("txtName", "Width") & "/" & fvForm.GetProperty("cmdOK", "Default") & "/" & fvForm.GetProperty("chkHidden", "Value") & "/" & fvForm.GetProperty("cmdOK", "Caption") & "/" & fvForm.Selected("cmdOK") & "/" & fvForm.Selected("txtName")
End Sub

Sub fvForm_MouseDown(Button As Integer, Shift As Integer, X As Integer, Y As Integer)
    Note "[" & fvForm.ControlAt(X, Y) & "|" & fvForm.HandleAt(X, Y) & "]"
End Sub

Sub fvForm_DblClick()
    Note "dbl"
End Sub

Sub cmdTab_Click()
    fvForm.TabNumbers = True
    fvForm.ScrollX = 1
    fvForm.ScrollY = 1
End Sub
