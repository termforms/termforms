' model.bas -- the form being designed: its controls in tab order, their
' property values as written in the form block, and the comments kept with
' them. The block is read from text and written back to text always the
' same way, so a change shows in a diff as only the lines it touched.

' One control, or the form itself (always ctls(0)).
Public Type Ctl
    Name As String
    Kind As String          ' "Form", "CommandButton", ...
    Container As String     ' "" for the form's own; else the Frame's or bar menu's name
    Note As String          ' comment lines before its Begin, joined with vbLf
    EndNote As String       ' comment lines before its End
End Type

Public ctls() As Ctl
Public ctlCount As Integer

' "cmdOK.Caption" -> the value as written: "&OK" with its quotes, 10, True.
Public props As Dictionary(Of String)
' "cmdOK.Caption" -> the comment lines written before that line.
Public propNotes As Dictionary(Of String)

Public Sub ClearModel()
    Erase ctls
    ctlCount = 0
    props.Clear
    propNotes.Clear
End Sub

Public Function IndexOf(name As String) As Integer
    For i As Integer = 0 To ctlCount - 1
        If ctls(i).Name = name Then Return i
    Next
    Return -1
End Function

Public Function KindOf(name As String) As String
    Dim i As Integer = IndexOf(name)
    If i < 0 Then Return ""
    Return ctls(i).Kind
End Function

Public Function FormName() As String
    If ctlCount = 0 Then Return ""
    Return ctls(0).Name
End Function

' -- Values -----------------------------------------------------------------

' A value as written, without its quotes: what SetProperty takes.
Public Function Plain(literal As String) As String
    If Left$(literal, 1) = Chr$(34) And Len(literal) >= 2 Then
        Return Replace(Mid$(literal, 2, Len(literal) - 2), Chr$(34) & Chr$(34), Chr$(34))
    End If
    Return literal
End Function

' Text as a form block writes it for the property's type.
Public Function Literal(kind As String, prop As String, text As String) As String
    Select Case PropType(kind, prop)
        Case "String"
            Return Chr$(34) & Replace(text, Chr$(34), Chr$(34) & Chr$(34)) & Chr$(34)
        Case "Boolean"
            If LCase$(text) = "true" Then Return "True"
            Return "False"
        Case Else
            Return text
    End Select
End Function

' The value as text: the one written, else the runtime's default.
Public Function ValueOf(name As String, prop As String) As String
    Dim key As String = name & "." & prop
    If props.Exists(key) Then Return Plain(props(key))
    Return DefaultOf(KindOf(name), prop)
End Function

Public Function NumberOf(name As String, prop As String) As Integer
    Return CInt(Val(ValueOf(name, prop)))
End Function

Public Sub SetValue(name As String, prop As String, text As String)
    props(name & "." & prop) = Literal(KindOf(name), prop, text)
End Sub

' -- Reading a form block ---------------------------------------------------

' A comment line (' or Rem), or blank.
Private Function IsNote(t As String) As Boolean
    Return Left$(t, 1) = "'" Or LCase$(Left$(t, 4)) = "rem " Or LCase$(t) = "rem"
End Function

' The value part of "Prop = value  ' comment": a string literal through
' its closing quote (doubled quotes inside it), else up to a comment.
Private Function ValuePart(text As String) As String
    Dim t As String = Trim$(text)
    If Left$(t, 1) = Chr$(34) Then
        Dim i As Integer = 2
        Do While i <= Len(t)
            If Mid$(t, i, 1) = Chr$(34) Then
                If Mid$(t, i + 1, 1) = Chr$(34) Then
                    i = i + 2
                Else
                    Return Left$(t, i)
                End If
            Else
                i = i + 1
            End If
        Loop
        Return t
    End If
    Dim quote As Integer = InStr(t, "'")
    If quote > 0 Then t = Left$(t, quote - 1)
    Return Trim$(t)
End Function

Private Sub AddNote(ByRef notes As String, line As String)
    If notes = "" Then
        notes = line
    Else
        notes = notes & vbLf & line
    End If
End Sub

' Reads the block whose "Begin Form" is lines(first); last becomes the
' index of its End. Returns "" or what is wrong.
Public Function ParseBlock(lines() As String, first As Integer, ByRef last As Integer) As String
    ClearModel
    Dim stack(40) As String
    Dim depth As Integer = 0
    Dim pending As String = ""
    For i As Integer = first To UBound(lines)
        Dim t As String = Trim$(lines(i))
        If t = "" Then
            ' blank lines are not kept
        ElseIf IsNote(t) Then
            AddNote pending, t
        ElseIf LCase$(Left$(t, 6)) = "begin " Then
            Dim words() As String = Split(Trim$(Mid$(t, 7)), " ")
            If UBound(words) < 1 Or depth > 40 Then Return "line " & (i + 1) & ": expected Begin <type> <name>"
            Dim c As Ctl
            c.Kind = words(0)
            c.Name = Trim$(words(UBound(words)))
            c.Container = ""
            If depth > 1 Then c.Container = stack(depth - 1)
            c.Note = pending
            pending = ""
            ReDim Preserve ctls(ctlCount)
            ctls(ctlCount) = c
            ctlCount = ctlCount + 1
            stack(depth) = c.Name
            depth = depth + 1
        ElseIf LCase$(t) = "end" Or LCase$(Left$(t, 4)) = "end " Or LCase$(Left$(t, 4)) = "end'" Then
            If depth = 0 Then Return "line " & (i + 1) & ": End without Begin"
            depth = depth - 1
            ctls(IndexOf(stack(depth))).EndNote = pending
            pending = ""
            If depth = 0 Then
                last = i
                Return ""
            End If
        ElseIf InStr(t, "=") > 0 And depth > 0 Then
            Dim eq As Integer = InStr(t, "=")
            Dim key As String = stack(depth - 1) & "." & Trim$(Left$(t, eq - 1))
            props(key) = ValuePart(Mid$(t, eq + 1))
            If pending <> "" Then propNotes(key) = pending
            pending = ""
        Else
            Return "line " & (i + 1) & ": not part of a form block: " & t
        End If
    Next
    Return "the form block has no End"
End Function

' The first line of a form block in lines ("Begin Form ..."), or -1.
Public Function BlockStart(lines() As String) As Integer
    For i As Integer = 0 To UBound(lines)
        If LCase$(Left$(Trim$(lines(i)), 11)) = "begin form " Then Return i
    Next
    Return -1
End Function

' -- Writing it -------------------------------------------------------------

Private Sub Emit(ByRef out As String, indent As Integer, text As String)
    out = out & Space$(indent * 4) & text & vbLf
End Sub

Private Sub EmitNotes(ByRef out As String, indent As Integer, notes As String)
    If notes = "" Then Exit Sub
    For Each line As String In Split(notes, vbLf)
        Emit out, indent, line
    Next
End Sub

' Its properties: the catalog's order, only those that differ from the
' runtime's defaults (or carry a comment); then any the catalog does not
' know, as they were.
Private Sub EmitProps(ByRef out As String, indent As Integer, c As Ctl)
    Dim known As String = "," & PropOrder(c.Kind) & ","
    For Each p As String In Split(PropOrder(c.Kind), ",")
        Dim key As String = c.Name & "." & p
        If props.Exists(key) Then
            Dim keep As Boolean = Plain(props(key)) <> DefaultOf(c.Kind, p)
            If propNotes.Exists(key) Then
                keep = True
                EmitNotes out, indent, propNotes(key)
            End If
            If keep Then Emit out, indent, p & " = " & props(key)
        End If
    Next
    Dim prefix As String = c.Name & "."
    For Each key As String In props.Keys
        If Left$(key, Len(prefix)) = prefix Then
            Dim p As String = Mid$(key, Len(prefix) + 1)
            If InStr(known, "," & p & ",") = 0 Then
                If propNotes.Exists(key) Then EmitNotes out, indent, propNotes(key)
                Emit out, indent, p & " = " & props(key)
            End If
        End If
    Next
End Sub

Private Sub EmitCtl(ByRef out As String, indent As Integer, at As Integer)
    Dim c As Ctl = ctls(at)
    EmitNotes out, indent, c.Note
    Emit out, indent, "Begin " & c.Kind & " " & c.Name
    EmitProps out, indent + 1, c
    For i As Integer = 1 To ctlCount - 1
        If ctls(i).Container = c.Name Then EmitCtl out, indent + 1, i
    Next
    EmitNotes out, indent + 1, c.EndNote
    Emit out, indent, "End"
End Sub

' The whole block, lines ending in vbLf.
Public Function BlockText() As String
    Dim out As String = ""
    If ctlCount = 0 Then Return ""
    Dim f As Ctl = ctls(0)
    EmitNotes out, 0, f.Note
    Emit out, 0, "Begin Form " & f.Name
    EmitProps out, 1, f
    For i As Integer = 1 To ctlCount - 1
        If ctls(i).Container = "" Then EmitCtl out, 1, i
    Next
    EmitNotes out, 1, f.EndNote
    Emit out, 0, "End"
    Return out
End Function

' -- Changing it ------------------------------------------------------------

' Whether name is inside container (directly, or through Frames).
Public Function IsInside(name As String, container As String) As Boolean
    Dim at As String = ctls(IndexOf(name)).Container
    Do While at <> ""
        If at = container Then Return True
        at = ctls(IndexOf(at)).Container
    Loop
    Return False
End Function

' The index just past name and everything inside it.
Public Function BlockEnd(at As Integer) As Integer
    Dim i As Integer = at + 1
    Do While i < ctlCount
        If Not IsInside(ctls(i).Name, ctls(at).Name) Then Exit Do
        i = i + 1
    Loop
    Return i
End Function

' Moves ctls(from) .. ctls(upTo - 1) to sit before index target (given
' as an index of the array before the move).
Public Sub MoveRange(from As Integer, upTo As Integer, target As Integer)
    If target >= from And target <= upTo Then Exit Sub
    Dim moving() As Ctl
    ReDim moving(upTo - from - 1)
    For i As Integer = from To upTo - 1
        moving(i - from) = ctls(i)
    Next
    Dim rest() As Ctl
    ReDim rest(ctlCount - (upTo - from) - 1)
    Dim n As Integer = 0
    Dim placeAt As Integer = -1
    For i As Integer = 0 To ctlCount - 1
        If i = target Then placeAt = n
        If i < from Or i >= upTo Then
            rest(n) = ctls(i)
            n = n + 1
        End If
    Next
    If placeAt < 0 Then placeAt = n
    n = 0
    Dim k As Integer = 0
    For i As Integer = 0 To ctlCount - 1
        If i >= placeAt And k <= UBound(moving) Then
            ctls(i) = moving(k)
            k = k + 1
        Else
            ctls(i) = rest(n)
            n = n + 1
        End If
    Next
End Sub

' Where a new control (or one moved in) goes in container: after the last
' thing already in it, so it comes last in its tab order.
Public Function EndOf(container As String) As Integer
    If container = "" Then Return ctlCount
    Return BlockEnd(IndexOf(container))
End Function

' Adds a control at the end of its container; returns its index.
Public Function AddCtl(kind As String, name As String, container As String) As Integer
    Dim c As Ctl
    c.Kind = kind
    c.Name = name
    c.Container = container
    Dim target As Integer = EndOf(container)
    ReDim Preserve ctls(ctlCount)
    ctls(ctlCount) = c
    ctlCount = ctlCount + 1
    MoveRange ctlCount - 1, ctlCount, target
    Return IndexOf(name)
End Function

' Removes a control and everything inside it, with their values.
Public Sub RemoveCtl(name As String)
    Dim at As Integer = IndexOf(name)
    Dim upTo As Integer = BlockEnd(at)
    For i As Integer = at To upTo - 1
        DropProps ctls(i).Name
    Next
    Dim keep() As Ctl
    Dim n As Integer = 0
    For i As Integer = 0 To ctlCount - 1
        If i < at Or i >= upTo Then
            ReDim Preserve keep(n)
            keep(n) = ctls(i)
            n = n + 1
        End If
    Next
    ctls = keep
    ctlCount = n
End Sub

Private Sub DropProps(name As String)
    Dim prefix As String = name & "."
    Dim gone() As String
    Dim n As Integer = 0
    For Each key As String In props.Keys
        If Left$(key, Len(prefix)) = prefix Then
            ReDim Preserve gone(n)
            gone(n) = key
            n = n + 1
        End If
    Next
    For i As Integer = 0 To n - 1
        props.Remove gone(i)
        If propNotes.Exists(gone(i)) Then propNotes.Remove gone(i)
    Next
End Sub

' Renames a control: its values and whatever it contains follow.
Public Sub RenameCtl(oldName As String, newName As String)
    Dim fresh As Dictionary(Of String)
    Dim notes As Dictionary(Of String)
    Dim prefix As String = oldName & "."
    For Each key As String In props.Keys
        Dim k As String = key
        If Left$(key, Len(prefix)) = prefix Then k = newName & "." & Mid$(key, Len(prefix) + 1)
        fresh(k) = props(key)
        If propNotes.Exists(key) Then notes(k) = propNotes(key)
    Next
    props = fresh
    propNotes = notes
    For i As Integer = 0 To ctlCount - 1
        If ctls(i).Name = oldName Then ctls(i).Name = newName
        If ctls(i).Container = oldName Then ctls(i).Container = newName
    Next
End Sub

' A name nothing uses yet: the kind's prefix and the first free number.
Public Function NewName(kind As String) As String
    Dim prefix As String
    Select Case kind
        Case "Label"
            prefix = "lbl"
        Case "TextBox"
            prefix = "txt"
        Case "CommandButton"
            prefix = "cmd"
        Case "CheckBox"
            prefix = "chk"
        Case "OptionButton"
            prefix = "opt"
        Case "Frame"
            prefix = "fra"
        Case "ListBox"
            prefix = "lst"
        Case "ComboBox"
            prefix = "cbo"
        Case "Timer"
            prefix = "tmr"
        Case "Menu"
            prefix = "mnu"
        Case "FormView"
            prefix = "fv"
        Case Else
            prefix = "ctl"
    End Select
    Dim n As Integer = 1
    Do While IndexOf(prefix & n) >= 0
        n = n + 1
    Loop
    Return prefix & n
End Function

' A name BASIC accepts: a letter, then letters, digits and _.
Public Function IsBasicName(name As String) As Boolean
    If name = "" Then Return False
    For i As Integer = 1 To Len(name)
        Dim ch As String = Mid$(name, i, 1)
        Dim letter As Boolean = (ch >= "a" And ch <= "z") Or (ch >= "A" And ch <= "Z")
        If i = 1 And Not letter Then Return False
        If Not letter And Not (ch >= "0" And ch <= "9") And ch <> "_" Then Return False
    Next
    Return True
End Function

' -- Where things are in the view --------------------------------------------

' Whether the form shows a menu bar (a visible bar menu).
Public Function HasMenuBar() As Boolean
    For i As Integer = 1 To ctlCount - 1
        If ctls(i).Kind = "Menu" And ctls(i).Container = "" Then
            If LCase$(ValueOf(ctls(i).Name, "Visible")) <> "false" Then Return True
        End If
    Next
    Return False
End Function

' Where a container's inside starts, in the view's cells (the form's
' corner at -scrollX, -scrollY).
Public Function InsideX(container As String, scrollX As Integer) As Integer
    If container = "" Then Return 1 - scrollX
    Dim at As Integer = IndexOf(container)
    Return InsideX(ctls(at).Container, scrollX) + NumberOf(container, "Left") + 1
End Function

Public Function InsideY(container As String, scrollY As Integer) As Integer
    If container = "" Then
        If HasMenuBar() Then Return 2 - scrollY
        Return 1 - scrollY
    End If
    Dim at As Integer = IndexOf(container)
    Return InsideY(ctls(at).Container, scrollY) + NumberOf(container, "Top") + 1
End Function

' The innermost Frame whose inside holds cell x, y of the view, leaving
' out name and what is inside it; "" for the form.
Public Function FrameAt(x As Integer, y As Integer, name As String, sx As Integer, sy As Integer) As String
    Dim best As String = ""
    For i As Integer = 1 To ctlCount - 1
        Dim f As String = ctls(i).Name
        If ctls(i).Kind = "Frame" And f <> name Then
            If Not IsInside(f, name) Then
                Dim fx As Integer = InsideX(f, sx)
                Dim fy As Integer = InsideY(f, sy)
                If x >= fx And y >= fy And x < fx + NumberOf(f, "Width") - 2 And y < fy + NumberOf(f, "Height") - 2 Then
                    If best = "" Then
                        best = f
                    ElseIf IsInside(f, best) Then
                        best = f
                    End If
                End If
            End If
        End If
    Next
    Return best
End Function
