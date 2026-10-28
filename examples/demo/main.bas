' main.bas -- the demo's main form: the menus, a button for each of the
' other forms, and the event log.
Begin Form frmDemo
    Caption = "TermForms demo"
    Width = 78
    Height = 22
    Begin Menu mnuFile
        Caption = "&File"
        Begin Menu mnuOpen
            Caption = "&Open..."
            Shortcut = "Ctrl+O"
        End
        Begin Menu mnuFileSep
            Caption = "-"
        End
        Begin Menu mnuExit
            Caption = "E&xit"
            Shortcut = "Ctrl+Q"
        End
    End
    Begin Menu mnuForms
        Caption = "F&orms"
        Begin Menu mnuControls
            Caption = "&Controls..."
            Shortcut = "F2"
        End
        Begin Menu mnuLists
            Caption = "&Lists..."
            Shortcut = "F3"
        End
        Begin Menu mnuTimer
            Caption = "&Timer and DoEvents..."
            Shortcut = "F5"
        End
    End
    Begin Menu mnuView
        Caption = "&View"
        Begin Menu mnuFocus
            Caption = "Show &focus events"
            Checked = True
        End
        Begin Menu mnuClear
            Caption = "&Clear the log"
            Shortcut = "Ctrl+L"
        End
    End
    Begin Menu mnuHelp
        Caption = "&Help"
        Begin Menu mnuKeys
            Caption = "&Keys"
            Shortcut = "F1"
        End
        Begin Menu mnuAbout
            Caption = "&About"
        End
    End
    Begin Label lblWelcome
        Caption = "Every v1 control, the menus, dialogs, Timers and the mouse. 中文 and 日本語 take two cells each."
        Left = 1
        Width = 28
        Height = 5
    End
    Begin CommandButton cmdControls
        Caption = "&Controls..."
        Left = 1
        Top = 6
        Width = 26
    End
    Begin CommandButton cmdLists
        Caption = "&Lists..."
        Left = 1
        Top = 8
        Width = 26
    End
    Begin CommandButton cmdTimer
        Caption = "&Timer, DoEvents..."
        Left = 1
        Top = 10
        Width = 26
    End
    Begin CommandButton cmdQuit
        Caption = "&Quit"
        Left = 1
        Top = 12
        Width = 26
        Cancel = True
    End
    Begin Label lblHint
        Caption = "F10 or Ctrl+B: menus. Ctrl+F4: close."
        Left = 1
        Top = 15
        Width = 28
        Height = 3
    End
    Begin Frame fraLog
        Caption = "Event log"
        Left = 30
        Width = 46
        Height = 19
        Begin ListBox lstLog
            Left = 1
            Width = 42
            Height = 17
        End
    End
End

Sub Form_Load()
    Note "frmDemo Load"
End Sub

Sub Form_Activate()
    Note "frmDemo Activate"
End Sub

Sub Form_Unload(ByRef Cancel As Integer)
    If MsgBox("Leave the demo?", vbYesNo + vbQuestion, "Quit") = vbNo Then
        Cancel = 1
        Note "frmDemo Unload refused"
    End If
End Sub

Sub cmdControls_Click()
    Note "cmdControls Click"
    frmControls.Show vbModal
    Note "back from Controls"
End Sub

Sub cmdLists_Click()
    Note "cmdLists Click"
    frmLists.Show vbModal
End Sub

Sub cmdTimer_Click()
    Note "cmdTimer Click"
    frmTimer.Show vbModal
End Sub

Sub cmdQuit_Click()
    Unload Me
End Sub

Sub cmdControls_GotFocus()
    FocusNote "cmdControls GotFocus"
End Sub

Sub cmdControls_LostFocus()
    FocusNote "cmdControls LostFocus"
End Sub

Sub lstLog_DblClick()
    MsgBox lstLog.Text, vbInformation, "Log line"
End Sub

Sub mnuFile_Click()
    Note "File menu opened"
End Sub

Sub mnuOpen_Click()
    Dim fileName As String = InputBox("A file to look at:", "Open", "notes.txt")
    If fileName = "" Then
        Note "Open cancelled"
    ElseIf Exists(fileName) Then
        Note "Open: " & fileName & " is " & FileLen(fileName) & " bytes"
    Else
        MsgBox "There is no file called " & fileName & ".", vbExclamation, "Open"
        Note "Open: no " & fileName
    End If
End Sub

Sub mnuExit_Click()
    Unload Me
End Sub

Sub mnuControls_Click()
    cmdControls_Click
End Sub

Sub mnuLists_Click()
    cmdLists_Click
End Sub

Sub mnuTimer_Click()
    cmdTimer_Click
End Sub

Sub mnuFocus_Click()
    mnuFocus.Checked = Not mnuFocus.Checked
    showFocus = mnuFocus.Checked
    Note "focus events " & ShownOrHidden(showFocus)
End Sub

Function ShownOrHidden(shown As Boolean) As String
    If shown Then
        Return "shown"
    End If
    Return "hidden"
End Function

Sub mnuClear_Click()
    lstLog.Clear
End Sub

Sub mnuKeys_Click()
    MsgBox "Tab, Shift+Tab: next, previous control" & vbLf & _
           "Arrows: within a group or a list" & vbLf & _
           "Space: press, toggle, select" & vbLf & _
           "Enter, Esc: the Default, Cancel button" & vbLf & _
           "Alt+letter: the underlined letter" & vbLf & _
           "F10 or Ctrl+B: the menu bar" & vbLf & _
           "Ctrl+F4: close the form", vbInformation, "Keys"
End Sub

Sub mnuAbout_Click()
    MsgBox "TermForms demo, built with tfc.", vbInformation, "About"
End Sub
