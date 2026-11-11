' window.bas -- the designer's form (its layout is in window.frm): the Toolbox
' and the form's Timers on the left, the form being designed in the
' middle (a FormView, drawn as it will run), its controls' properties and
' events on the right, and what is selected and which keys work below.

Dim sel As String              ' the selected control (or the form)
Dim tool As String             ' a control chosen in the Toolbox, to place with a click
Dim dragMode As String         ' "", "move" or "size": what a press on the view started
Dim dragName As String
Dim pressX As Integer
Dim pressY As Integer
Dim startA As Integer          ' Left (moving) or Width (sizing) when the press began
Dim startB As Integer          ' Top or Height
Dim dragged As Boolean         ' the mouse moved since the press (an undo step was taken)
Dim lastX As Integer           ' where the last press on the view was, for DblClick
Dim lastY As Integer
Dim tabMode As Boolean         ' clicks set the tab order
Dim lastPick As String         ' the control last clicked in tab-order mode
Dim dirty As Boolean           ' changed since saved
Dim updating As Boolean        ' code is filling the lists: their Click events are not the user's
Dim propRows() As String       ' the property on each row of lstProps ("Name" first)
Dim undoTexts() As String
Dim undoAt As Integer
Dim undoTop As Integer

' -- Starting --------------------------------------------------------------

Sub Form_Load()
    FillDefaults
    For i As Integer = 0 To kindCount - 1
        If kinds(i) <> "Form" And kinds(i) <> "Menu" Then lstTools.AddItem kinds(i)
    Next
    sel = FormName()
    ShowAll
    Status "Opened " & ShortName(SourcePath()) & "."
End Sub

' The runtime's defaults for every kind of control, from a FormView
' nobody sees: a control just added there gives them.
Sub FillDefaults()
    For i As Integer = 0 To kindCount - 1
        Dim kind As String = kinds(i)
        fvScratch.Clear
        fvScratch.AddControl "Form", "f"
        Dim who As String = "f"
        If kind <> "Form" Then
            fvScratch.AddControl kind, "c"
            who = "c"
        End If
        For Each p As String In Split(PropOrder(kind), ",")
            defaults(kind & "." & p) = fvScratch.GetProperty(who, p)
        Next
    Next
    fvScratch.Clear
End Sub

' -- Fitting the terminal ----------------------------------------------------

Sub Form_Resize()
    Dim w As Integer = Screen.Width
    Dim h As Integer = Screen.Height
    If w < 90 Then w = 90
    If h < 24 Then h = 24
    Me.Width = w
    Me.Height = h
    Dim inner As Integer = h - 3          ' rows inside the border, below the menu bar
    Dim rightSide As Integer = w - 2 - 30     ' where the right-hand panel starts
    fvForm.Left = 18
    fvForm.Width = rightSide - 19
    fvForm.Height = inner - 2
    lblCtl.Left = rightSide
    cboCtl.Left = rightSide
    lblProps.Left = rightSide
    lstProps.Left = rightSide
    lblValue.Left = rightSide
    txtValue.Left = rightSide
    cboValue.Left = rightSide
    lblEvents.Left = rightSide
    lstEvents.Left = rightSide
    Dim propsHeight As Integer = (inner - 2 - 6) * 2 \ 3
    lstProps.Height = propsHeight
    lblValue.Top = 3 + propsHeight
    txtValue.Top = 4 + propsHeight
    cboValue.Top = 4 + propsHeight
    lblEvents.Top = 5 + propsHeight
    lstEvents.Top = 6 + propsHeight
    lstEvents.Height = inner - 2 - lstEvents.Top
    lstTimers.Height = inner - 2 - 13 - 6
    lblForm.Top = lstTimers.Top + lstTimers.Height + 1
    cmdTabOrder.Top = lblForm.Top + 2
    lblStatus.Top = inner - 2
    lblKeys.Top = inner - 1
    lblStatus.Width = w - 2
    lblKeys.Width = w - 2
End Sub

' -- Showing the form and its parts ---------------------------------------------

Sub Status(text As String)
    lblStatus.Caption = Replace(text, "&", "&&")
End Sub

Sub ShowAll()
    ShowForm
    ShowControlList
    ShowProps
    ShowEvents
    ShowTimers
    ShowPlace
End Sub

' The view, rebuilt from the model: the form, then its controls in tab
' order with their values (a value the runtime refuses is left out here;
' it stays in the model as written).
Sub ShowForm()
    fvForm.Clear
    For i As Integer = 0 To ctlCount - 1
        Dim c As Ctl = ctls(i)
        Try
            fvForm.AddControl c.Kind, c.Name, c.Container
        Catch
            ' a control the view cannot hold (a Menu in a Frame): not shown
        End Try
        For Each p As String In Split(PropOrder(c.Kind), ",")
            If props.Exists(c.Name & "." & p) Then
                Try
                    fvForm.SetProperty c.Name, p, Plain(props(c.Name & "." & p))
                Catch
                End Try
            End If
        Next
    Next
    If IndexOf(sel) >= 0 Then fvForm.Selected(sel) = True
    fvForm.TabNumbers = tabMode
    Dim f As String = FormName()
    Dim modified As String = ""
    If dirty Then modified = " (changed)"
    Me.Caption = "TermForms Designer: " & ShortName(SourcePath()) & modified
    lblForm.Caption = Plain(f) & " " & NumberOf(f, "Width") & "x" & NumberOf(f, "Height")
End Sub

Sub ShowControlList()
    updating = True
    cboCtl.Clear
    For i As Integer = 0 To ctlCount - 1
        cboCtl.AddItem ctls(i).Name
        If ctls(i).Name = sel Then cboCtl.ListIndex = i
    Next
    updating = False
End Sub

Sub ShowTimers()
    lstTimers.Clear
    For i As Integer = 0 To ctlCount - 1
        If ctls(i).Kind = "Timer" Then lstTimers.AddItem ctls(i).Name
    Next
End Sub

' The selected control's properties: its name, then each one a form block
' may set, with its value (written, or the default).
Sub ShowProps()
    Dim kind As String = KindOf(sel)
    updating = True
    lstProps.Clear
    Erase propRows
    Dim n As Integer = 0
    ReDim propRows(0)
    propRows(0) = "Name"
    lstProps.AddItem Pad("Name") & sel
    For Each p As String In Split(PropOrder(kind), ",")
        If Designable(kind, p) Then
            n = n + 1
            ReDim Preserve propRows(n)
            propRows(n) = p
            lstProps.AddItem Pad(p) & ValueOf(sel, p)
        End If
    Next
    updating = False
    ShowValue
End Sub

Function Pad(text As String) As String
    Return Left$(text & Space$(17), 17) & " "
End Function

' The editor under the list for the property chosen in it: a TextBox, or
' a ComboBox of True and False for a Boolean.
Sub ShowValue()
    Dim row As Integer = lstProps.ListIndex
    If row < 0 Then row = 0
    Dim p As String = propRows(row)
    lblValue.Caption = p & ":"
    If p <> "Name" And PropType(KindOf(sel), p) = "Boolean" Then
        updating = True
        cboValue.Clear
        cboValue.AddItem "True"
        cboValue.AddItem "False"
        If LCase$(ValueOf(sel, p)) = "true" Then
            cboValue.ListIndex = 0
        Else
            cboValue.ListIndex = 1
        End If
        updating = False
        cboValue.Visible = True
        txtValue.Visible = False
    Else
        If p = "Name" Then
            txtValue.Text = sel
        Else
            txtValue.Text = ValueOf(sel, p)
        End If
        txtValue.Visible = True
        cboValue.Visible = False
    End If
End Sub

' The selected control's events: a * marks those with a handler in the .bas.
Sub ShowEvents()
    Dim kind As String = KindOf(sel)
    Dim code() As String
    ReadLines basPath, code
    lstEvents.Clear
    For Each e As String In Split(EventOrder(kind), ",")
        If e <> "" Then
            If HandlerIn(code, SubName(sel, e)) > 0 Then
                lstEvents.AddItem "* " & e
            Else
                lstEvents.AddItem "  " & e
            End If
        End If
    Next
End Sub

Sub ShowPlace()
    Dim kind As String = KindOf(sel)
    If sel = FormName() Then
        lblKeys.Caption = "Click a control to select it  Shift+arrows size the form  Toolbox: choose, then click on the form  F1 keys"
        Status sel & "  Form  " & NumberOf(sel, "Width") & " x " & NumberOf(sel, "Height")
    ElseIf kind = "Timer" Or kind = "Menu" Then
        lblKeys.Caption = "Del deletes  [ and ] select the previous or next control  F1 keys"
        Status sel & "  " & kind
    Else
        lblKeys.Caption = "Arrows move  Shift+arrows size  Del deletes  [ ] previous/next  Double-click: handler  F1 keys"
        Status sel & "  " & kind & "  Left " & NumberOf(sel, "Left") & "  Top " & NumberOf(sel, "Top") & "  Width " & NumberOf(sel, "Width") & "  Height " & NumberOf(sel, "Height")
    End If
End Sub

Sub SelectCtl(name As String)
    If IndexOf(name) < 0 Then name = FormName()
    sel = name
    ShowAll
End Sub

' -- Changes and undo ----------------------------------------------------------

' Before each change: the form as it stands, to go back to.
Sub Changing()
    ReDim Preserve undoTexts(undoAt)
    undoTexts(undoAt) = BlockText()
    undoAt = undoAt + 1
    undoTop = undoAt
    dirty = True
End Sub

Sub Restore(text As String)
    Dim lines() As String = Split(text, vbLf)
    Dim last As Integer = 0
    ParseBlock lines, 0, last
    If IndexOf(sel) < 0 Then sel = FormName()
    dirty = True
    ShowAll
End Sub

Sub mnuUndo_Click()
    If undoAt = 0 Then
        Status "Nothing to undo."
        Exit Sub
    End If
    If undoAt = undoTop Then
        ReDim Preserve undoTexts(undoTop)
        undoTexts(undoTop) = BlockText()
    End If
    undoAt = undoAt - 1
    Restore undoTexts(undoAt)
End Sub

Sub mnuRedo_Click()
    If undoAt >= undoTop Then
        Status "Nothing to redo."
        Exit Sub
    End If
    undoAt = undoAt + 1
    Restore undoTexts(undoAt)
End Sub

' -- Adding and removing controls ----------------------------------------------

' Adds a control of kind inside container at left, top (of that
' container's inside), with VB's first caption (its name), and selects it.
Sub PlaceCtl(kind As String, container As String, px As Integer, py As Integer)
    Changing
    Dim name As String = NewName(kind)
    AddCtl kind, name, container
    If kind <> "Timer" Then
        SetValue name, "Left", CStr(px)
        SetValue name, "Top", CStr(py)
    End If
    If InStr("," & PropOrder(kind) & ",", ",Caption,") > 0 Then SetValue name, "Caption", name
    tool = ""
    SelectCtl name
    Status "Added " & name & "."
End Sub

Sub ChooseTool(kind As String)
    If kind = "Timer" Then
        PlaceCtl "Timer", "", 0, 0
        Exit Sub
    End If
    tool = kind
    Status "Click on the form to place a " & kind & " (Esc: never mind)."
End Sub

Sub lstTools_DblClick()
    ChooseTool lstTools.Text
End Sub

Sub lstTools_KeyPress(ByRef KeyAscii As Integer)
    If KeyAscii = 13 And lstTools.ListIndex >= 0 Then
        KeyAscii = 0
        ' From the keyboard: at the form's top left, to move with the arrows.
        If lstTools.Text = "Timer" Then
            PlaceCtl "Timer", "", 0, 0
        Else
            PlaceCtl lstTools.Text, "", 1, 1
        End If
        fvForm.SetFocus
    End If
End Sub

Sub lstTools_Click()
    If Not updating And lstTools.ListIndex >= 0 Then ChooseTool lstTools.Text
End Sub

Sub AddFromMenu(kind As String)
    If kind = "Timer" Then
        PlaceCtl kind, "", 0, 0
    Else
        PlaceCtl kind, "", 1, 1
    End If
    fvForm.SetFocus
End Sub

Sub mnuAddLabel_Click()
    AddFromMenu "Label"
End Sub

Sub mnuAddTextBox_Click()
    AddFromMenu "TextBox"
End Sub

Sub mnuAddCommandButton_Click()
    AddFromMenu "CommandButton"
End Sub

Sub mnuAddCheckBox_Click()
    AddFromMenu "CheckBox"
End Sub

Sub mnuAddOptionButton_Click()
    AddFromMenu "OptionButton"
End Sub

Sub mnuAddFrame_Click()
    AddFromMenu "Frame"
End Sub

Sub mnuAddListBox_Click()
    AddFromMenu "ListBox"
End Sub

Sub mnuAddComboBox_Click()
    AddFromMenu "ComboBox"
End Sub

Sub mnuAddTimer_Click()
    AddFromMenu "Timer"
End Sub

Sub DeleteSelected()
    If sel = FormName() Then
        Status "The form itself cannot be deleted."
        Exit Sub
    End If
    Changing
    Dim gone As String = sel
    Dim at As Integer = IndexOf(sel)
    RemoveCtl sel
    If at >= ctlCount Then at = ctlCount - 1
    SelectCtl ctls(at).Name
    Status "Deleted " & gone & " (Ctrl+Z brings it back)."
End Sub

Sub mnuDelete_Click()
    DeleteSelected
End Sub

' -- The mouse on the view --------------------------------------------------------

Sub fvForm_MouseDown(Button As Integer, Shift As Integer, X As Integer, Y As Integer)
    lastX = X
    lastY = Y
    dragMode = ""
    If Button <> vbLeftButton Then Exit Sub
    Dim sx As Integer = fvForm.ScrollX
    Dim sy As Integer = fvForm.ScrollY
    If tool <> "" Then
        If fvForm.ControlAt(X, Y) = "" Then
            Status "Click on the form to place the " & tool & "."
            Exit Sub
        End If
        Dim inside As String = FrameAt(X, Y, "", sx, sy)
        PlaceCtl tool, inside, X - InsideX(inside, sx), Y - InsideY(inside, sy)
        Exit Sub
    End If
    Dim hit As String = fvForm.ControlAt(X, Y)
    If tabMode Then
        PickTabOrder hit
        Exit Sub
    End If
    Dim handle As String = fvForm.HandleAt(X, Y)
    If handle <> "" Then
        dragMode = "size"
        dragName = handle
        startA = NumberOf(handle, "Width")
        startB = NumberOf(handle, "Height")
    ElseIf hit <> "" Then
        If hit <> sel Then SelectCtl hit
        If hit <> FormName() And KindOf(hit) <> "Menu" Then
            dragMode = "move"
            dragName = hit
            startA = NumberOf(hit, "Left")
            startB = NumberOf(hit, "Top")
        End If
    End If
    pressX = X
    pressY = Y
    dragged = False
End Sub

Sub fvForm_MouseMove(Button As Integer, Shift As Integer, X As Integer, Y As Integer)
    If dragMode = "" Then Exit Sub
    If X = pressX And Y = pressY And Not dragged Then Exit Sub
    If Not dragged Then
        Changing
        dragged = True
    End If
    If dragMode = "move" Then
        SetValue dragName, "Left", CStr(startA + X - pressX)
        SetValue dragName, "Top", CStr(startB + Y - pressY)
    Else
        SetValue dragName, "Width", CStr(Larger(1, startA + X - pressX))
        SetValue dragName, "Height", CStr(Larger(1, startB + Y - pressY))
    End If
    ShowForm
    ShowPlace
End Sub

Function Larger(a As Integer, b As Integer) As Integer
    If a > b Then Return a
    Return b
End Function

' A move ends: the control goes into the Frame its top-left corner is
' now in (or out of the one it was in), keeping where it is on screen.
Sub fvForm_MouseUp(Button As Integer, Shift As Integer, X As Integer, Y As Integer)
    Dim mode As String = dragMode
    dragMode = ""
    If Not dragged Then Exit Sub
    If mode = "move" Then
        Dim sx As Integer = fvForm.ScrollX
        Dim sy As Integer = fvForm.ScrollY
        Dim c As Ctl = ctls(IndexOf(dragName))
        Dim ax As Integer = InsideX(c.Container, sx) + NumberOf(c.Name, "Left")
        Dim ay As Integer = InsideY(c.Container, sy) + NumberOf(c.Name, "Top")
        Dim target As String = FrameAt(ax, ay, c.Name, sx, sy)
        If target <> c.Container Then
            Dim at As Integer = IndexOf(c.Name)
            Dim upTo As Integer = BlockEnd(at)
            MoveRange at, upTo, EndOf(target)
            ctls(IndexOf(c.Name)).Container = target
            SetValue c.Name, "Left", CStr(ax - InsideX(target, sx))
            SetValue c.Name, "Top", CStr(ay - InsideY(target, sy))
            If target = "" Then
                Status c.Name & " is now on the form."
            Else
                Status c.Name & " is now in " & target & "."
            End If
        End If
    End If
    ShowAll
End Sub

' A double-click writes the control's usual handler (its first event).
Sub fvForm_DblClick()
    Dim hit As String = fvForm.ControlAt(lastX, lastY)
    If hit = "" Or tool <> "" Or tabMode Then Exit Sub
    WriteHandler hit, FirstEvent(KindOf(hit))
End Sub

' -- The keyboard on the view ----------------------------------------------------

Sub fvForm_KeyDown(ByRef KeyCode As Integer, Shift As Integer)
    Dim dx As Integer = 0
    Dim dy As Integer = 0
    Select Case KeyCode
        Case vbKeyLeft
            dx = -1
        Case vbKeyRight
            dx = 1
        Case vbKeyUp
            dy = -1
        Case vbKeyDown
            dy = 1
        Case vbKeyDelete
            KeyCode = 0
            DeleteSelected
            Exit Sub
        Case vbKeyEscape
            KeyCode = 0
            If tool <> "" Then
                tool = ""
                Status "Not placing anything."
            ElseIf tabMode Then
                mnuTabOrder_Click
            End If
            Exit Sub
        Case vbKeyReturn
            KeyCode = 0
            lstProps.SetFocus
            Exit Sub
        Case Else
            Exit Sub
    End Select
    KeyCode = 0
    Dim kind As String = KindOf(sel)
    If kind = "Timer" Or kind = "Menu" Then Exit Sub
    Dim sizing As Boolean = Shift Mod 2 = 1   ' vbShiftMask
    If sel = FormName() And Not sizing Then Exit Sub
    Changing
    If sizing Then
        SetValue sel, "Width", CStr(Larger(1, NumberOf(sel, "Width") + dx))
        SetValue sel, "Height", CStr(Larger(1, NumberOf(sel, "Height") + dy))
    Else
        SetValue sel, "Left", CStr(NumberOf(sel, "Left") + dx)
        SetValue sel, "Top", CStr(NumberOf(sel, "Top") + dy)
    End If
    ShowAll
End Sub

Sub fvForm_KeyPress(ByRef KeyAscii As Integer)
    Dim at As Integer = IndexOf(sel)
    If KeyAscii = Asc("[") Then
        KeyAscii = 0
        If at > 0 Then SelectCtl ctls(at - 1).Name
    ElseIf KeyAscii = Asc("]") Then
        KeyAscii = 0
        If at < ctlCount - 1 Then SelectCtl ctls(at + 1).Name
    End If
End Sub

' -- Tab order --------------------------------------------------------------------

Sub mnuTabOrder_Click()
    tabMode = Not tabMode
    lastPick = ""
    ShowForm
    If tabMode Then
        Status "Tab order: click the controls in the order wanted (in each Frame, its own); Esc or F6 ends."
        fvForm.SetFocus
    Else
        Status "Tab order set."
    End If
End Sub

Sub cmdTabOrder_Click()
    mnuTabOrder_Click
End Sub

' The control clicked comes next after the one clicked before it (when
' they share a container), else first in its container.
Sub PickTabOrder(name As String)
    If name = "" Or name = FormName() Then Exit Sub
    Dim kind As String = KindOf(name)
    If kind = "Menu" Or kind = "Timer" Then Exit Sub
    Changing
    Dim at As Integer = IndexOf(name)
    Dim upTo As Integer = BlockEnd(at)
    Dim container As String = ctls(at).Container
    Dim target As Integer
    If lastPick <> "" And IndexOf(lastPick) >= 0 And lastPick <> name Then
        If ctls(IndexOf(lastPick)).Container = container Then
            target = BlockEnd(IndexOf(lastPick))
        Else
            target = FirstIn(container)
        End If
    Else
        target = FirstIn(container)
    End If
    MoveRange at, upTo, target
    lastPick = name
    sel = name
    ShowAll
End Sub

' The index of the first control (not a menu) in container.
Function FirstIn(container As String) As Integer
    For i As Integer = 1 To ctlCount - 1
        If ctls(i).Container = container And ctls(i).Kind <> "Menu" Then Return i
    Next
    Return EndOf(container)
End Function

' -- The property sheet --------------------------------------------------------

Sub cboCtl_Click()
    If Not updating And cboCtl.ListIndex >= 0 Then SelectCtl cboCtl.Text
End Sub

Sub lstTimers_Click()
    If lstTimers.ListIndex >= 0 Then SelectCtl lstTimers.Text
End Sub

Sub lstProps_Click()
    If Not updating Then ShowValue
End Sub

Sub lstProps_KeyPress(ByRef KeyAscii As Integer)
    If KeyAscii = 13 Then
        KeyAscii = 0
        If txtValue.Visible Then
            txtValue.SetFocus
        Else
            cboValue.SetFocus
        End If
    End If
End Sub

Sub lstProps_DblClick()
    If txtValue.Visible Then
        txtValue.SetFocus
    Else
        cboValue.SetFocus
    End If
End Sub

Sub txtValue_KeyPress(ByRef KeyAscii As Integer)
    If KeyAscii = 13 Then
        KeyAscii = 0
        ApplyValue txtValue.Text
    ElseIf KeyAscii = 27 Then
        KeyAscii = 0
        ShowValue
        lstProps.SetFocus
    End If
End Sub

Sub cboValue_Click()
    If Not updating And cboValue.ListIndex >= 0 Then ApplyValue cboValue.Text
End Sub

' The value typed for the chosen property: the name (a new one), or a
' value the runtime is asked to take first (one it refuses is not kept).
Sub ApplyValue(text As String)
    Dim row As Integer = lstProps.ListIndex
    If row < 0 Then row = 0
    Dim p As String = propRows(row)
    If p = "Name" Then
        RenameSelected text
        Exit Sub
    End If
    Try
        fvForm.SetProperty sel, p, text
    Catch
        MsgBox p & " cannot be " & Chr$(34) & text & Chr$(34) & " (" & Err.Description & ").", vbExclamation, "Designer"
        ShowValue
        Exit Sub
    End Try
    Changing
    SetValue sel, p, text
    Dim keep As Integer = row
    ShowAll
    lstProps.ListIndex = keep
    Status sel & "." & p & " is now " & text & "."
End Sub

Sub RenameSelected(name As String)
    If name = sel Then Exit Sub
    If Not IsBasicName(name) Then
        MsgBox Chr$(34) & name & Chr$(34) & " is not a name BASIC accepts (a letter, then letters, digits or _).", vbExclamation, "Designer"
        ShowValue
        Exit Sub
    End If
    If IndexOf(name) >= 0 Then
        MsgBox "There is already a control called " & name & ".", vbExclamation, "Designer"
        ShowValue
        Exit Sub
    End If
    Changing
    Dim was As String = sel
    RenameCtl sel, name
    sel = name
    ShowAll
    If was <> FormName() And HandlerLine(SubName(was, FirstEvent(KindOf(name)))) > 0 Then
        Status "Renamed " & was & " to " & name & ". Its handlers in " & ShortName(basPath) & " keep the old name: rename them there."
    Else
        Status "Renamed " & was & " to " & name & "."
    End If
End Sub

' -- Event handlers ----------------------------------------------------------

' The handler's name: the form's events are Form_ whatever it is called.
Function FirstEvent(kind As String) As String
    Dim names() As String = Split(EventOrder(kind), ",")
    Return names(0)
End Function

Function SubName(name As String, eventName As String) As String
    If name = FormName() Then Return "Form_" & eventName
    Return name & "_" & eventName
End Function

Sub WriteHandler(name As String, eventName As String)
    If eventName = "" Then Exit Sub
    Dim subText As String = SubName(name, eventName)
    Dim at As Integer = HandlerLine(subText)
    If at > 0 Then
        Status "Sub " & subText & " is at " & ShortName(basPath) & ":" & at & "."
    Else
        at = AddHandler(subText, EventParams(KindOf(name), eventName))
        Status "Wrote Sub " & subText & " at " & ShortName(basPath) & ":" & at & "."
    End If
    ShowEvents
End Sub

Sub lstEvents_DblClick()
    If lstEvents.ListIndex >= 0 Then WriteHandler sel, Trim$(Mid$(lstEvents.Text, 3))
End Sub

Sub lstEvents_KeyPress(ByRef KeyAscii As Integer)
    If KeyAscii = 13 And lstEvents.ListIndex >= 0 Then
        KeyAscii = 0
        WriteHandler sel, Trim$(Mid$(lstEvents.Text, 3))
    End If
End Sub

' F7: the code, in the programmer's own editor ($VISUAL, else $EDITOR),
' at the selected control's first handler if it has one.
Sub mnuCode_Click()
    Dim at As Integer = 1
    For Each e As String In Split(EventOrder(KindOf(sel)), ",")
        If at = 1 And e <> "" Then
            Dim found As Integer = HandlerLine(SubName(sel, e))
            If found > 0 Then at = found
        End If
    Next
    Dim editor As String = Environ$("VISUAL")
    If editor = "" Then editor = Environ$("EDITOR")
    If editor = "" Then
        Status "The code is in " & basPath & ", line " & at & " (set EDITOR to open it from here)."
        Exit Sub
    End If
    Shell editor & " +" & at & " " & Quoted(basPath)
    ShowEvents
End Sub

' -- Files --------------------------------------------------------------------

Sub mnuSave_Click()
    SaveNow
End Sub

Function SaveNow() As Boolean
    If ChangedOnDisk() Then
        If inFrm Then
            Dim answer As Integer = MsgBox(ShortName(frmPath) & " was changed by something else since it was opened. Replace it with this form?", vbYesNo + vbExclamation, "Designer")
            If answer <> vbYes Then Return False
        End If
    End If
    Try
        SaveForm
    Catch
        MsgBox "Could not save " & ShortName(SourcePath()) & ": " & Err.Description, vbCritical, "Designer"
        Return False
    End Try
    dirty = False
    ShowForm
    Status "Saved " & ShortName(SourcePath()) & "."
    Return True
End Function

Sub mnuToFrm_Click()
    If inFrm Then
        Status "The form is already in " & ShortName(frmPath) & "."
        Exit Sub
    End If
    Dim answer As Integer = MsgBox("Move the form block out of " & ShortName(basPath) & " into " & ShortName(frmPath) & " (saving the form as it is now)?", vbYesNo + vbQuestion, "Designer")
    If answer <> vbYes Then Exit Sub
    Try
        MoveToFrm
    Catch
        MsgBox "Could not move it: " & Err.Description, vbCritical, "Designer"
        Exit Sub
    End Try
    dirty = False
    ShowForm
    Status "The form is now in " & ShortName(frmPath) & "."
End Sub

Sub mnuOpen_Click()
    If Not SavedOrDropped() Then Exit Sub
    Dim named As String = InputBox("The form's .bas or .frm file:", "Open", basPath)
    If named = "" Then Exit Sub
    NameFiles named
    Dim problem As String = LoadForm()
    If problem <> "" Then
        MsgBox problem, vbExclamation, "Designer"
        Exit Sub
    End If
    dirty = False
    undoAt = 0
    undoTop = 0
    tool = ""
    SelectCtl FormName()
    Status "Opened " & ShortName(SourcePath()) & "."
End Sub

' Unsaved changes: save them, drop them, or stay (False).
Function SavedOrDropped() As Boolean
    If Not dirty Then Return True
    Dim answer As Integer = MsgBox("Save the changes to " & ShortName(SourcePath()) & "?", vbYesNoCancel + vbQuestion, "Designer")
    If answer = vbCancel Then Return False
    If answer = vbYes Then Return SaveNow()
    Return True
End Function

Sub mnuExit_Click()
    Unload Me
End Sub

Sub Form_Unload(ByRef Cancel As Integer)
    If Not SavedOrDropped() Then Cancel = 1
End Sub

Sub mnuKeys_Click()
    Dim text As String = "On the form: click to select; drag to move; drag the handle to size; "
    text = text & "arrows move, Shift+arrows size, Del deletes, [ and ] step through the controls, "
    text = text & "Enter goes to the properties. Double-click a control (or Enter on an event) to write its handler." & vbLf & vbLf
    text = text & "Toolbox: choose a control, then click on the form to place it (Enter puts it at the top left)." & vbLf
    text = text & "F6 tab order: click the controls in order. F7 the code, in $VISUAL or $EDITOR." & vbLf
    text = text & "Ctrl+S save, Ctrl+Z undo, Ctrl+Y redo, Ctrl+Q quit."
    MsgBox text, vbInformation, "Keys"
End Sub
