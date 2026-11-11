' files.bas -- the files a form lives in: its .bas (the code, and maybe
' the form block) and its .frm (the block alone). Reading, saving safely,
' and writing event handlers' first lines into the .bas.

Public basPath As String      ' main.bas
Public frmPath As String      ' main.frm
Public inFrm As Boolean       ' the block is (or will be) in the .frm
Public loadedText As String   ' the file the block came from, as read, to notice changes

' A file's lines (LF or CR LF); none if it is not there.
Public Function ReadLines(path As String, ByRef lines() As String) As Integer
    Erase lines
    If Not Exists(path) Then Return 0
    Dim n As Integer = 0
    Dim f As Integer = FreeFile
    Open path For Input As #f
    Do While Not EOF(f)
        Dim line As String
        Line Input #f, line
        ReDim Preserve lines(n)
        lines(n) = line
        n = n + 1
    Loop
    Close #f
    Return n
End Function

Public Function ReadText(path As String) As String
    Dim lines() As String
    If ReadLines(path, lines) = 0 Then Return ""
    Return Join(lines, vbLf) & vbLf
End Function

' Writes text to path without ever leaving it half written: to a new
' file beside it first, then the old one is moved aside, the new one put
' in its place, and the old one removed.
Public Sub WriteSafely(path As String, text As String)
    Dim fresh As String = path & ".tfdesign-new"
    Dim old As String = path & ".tfdesign-old"
    If Exists(fresh) Then Kill fresh
    Dim f As Integer = FreeFile
    Open fresh For Output As #f
    Print #f, text;
    Close #f
    If Exists(old) Then Kill old
    If Exists(path) Then Name path As old
    Name fresh As path
    If Exists(old) Then Kill old
End Sub

' Sets basPath and frmPath from what the user named (either, or neither
' existing yet).
Public Sub NameFiles(named As String)
    Dim stem As String = named
    Dim dot As Integer = InStrRev(named, ".")
    Dim slash As Integer = InStrRev(Replace(named, "\", "/"), "/")
    If dot > slash Then stem = Left$(named, dot - 1)
    basPath = stem & ".bas"
    frmPath = stem & ".frm"
End Sub

' The module's name: the file's, without its folder or .bas.
Public Function ModuleName() As String
    Dim stem As String = Left$(basPath, Len(basPath) - 4)
    Dim slash As Integer = InStrRev(Replace(stem, "\", "/"), "/")
    Return Mid$(stem, slash + 1)
End Function

Public Function ShortName(path As String) As String
    Dim slash As Integer = InStrRev(Replace(path, "\", "/"), "/")
    Return Mid$(path, slash + 1)
End Function

' Reads the form: the .frm if there is one, else the block in the .bas,
' else a new form (to be saved in a .frm). Returns "" or what is wrong.
Public Function LoadForm() As String
    Dim lines() As String
    Dim last As Integer = 0
    If Exists(frmPath) Then
        inFrm = True
        ReadLines frmPath, lines
        loadedText = ReadText(frmPath)
    Else
        inFrm = False
        ReadLines basPath, lines
        loadedText = ReadText(basPath)
    End If
    Dim first As Integer = -1
    If UBound(lines) >= 0 Then first = BlockStart(lines)
    If first < 0 Then
        ' A new form, in a .frm of its own.
        inFrm = True
        loadedText = ReadText(frmPath)
        ClearModel
        AddCtl "Form", "frm" & ModuleName(), ""
        SetValue FormName(), "Caption", ModuleName()
        Return ""
    End If
    Dim problem As String = ParseBlock(lines, first, last)
    If problem <> "" Then Return ShortName(SourcePath()) & ", " & problem
    Return ""
End Function

Public Function SourcePath() As String
    If inFrm Then Return frmPath
    Return basPath
End Function

' Whether the file the block lives in changed since it was read.
Public Function ChangedOnDisk() As Boolean
    Return ReadText(SourcePath()) <> loadedText
End Function

' Saves the block: the whole .frm, or the block's lines in the .bas (read
' again now, so the code around it is kept as it stands).
Public Sub SaveForm()
    Dim block As String = BlockText()
    If inFrm Then
        WriteSafely frmPath, block
        loadedText = block
        Exit Sub
    End If
    Dim lines() As String
    ReadLines basPath, lines
    Dim first As Integer = -1
    If UBound(lines) >= 0 Then first = BlockStart(lines)
    Dim out As String = ""
    If first < 0 Then
        out = block & ReadText(basPath)
    Else
        Dim last As Integer = BlockEndLine(lines, first)
        For i As Integer = 0 To first - 1
            out = out & lines(i) & vbLf
        Next
        out = out & block
        For i As Integer = last + 1 To UBound(lines)
            out = out & lines(i) & vbLf
        Next
    End If
    WriteSafely basPath, out
    loadedText = out
End Sub

' The End of the block starting at lines(first), counting Begins and Ends.
Public Function BlockEndLine(lines() As String, first As Integer) As Integer
    Dim depth As Integer = 0
    For i As Integer = first To UBound(lines)
        Dim t As String = LCase$(Trim$(lines(i)))
        If Left$(t, 6) = "begin " Then
            depth = depth + 1
        ElseIf t = "end" Or Left$(t, 4) = "end " Or Left$(t, 4) = "end'" Then
            depth = depth - 1
            If depth = 0 Then Return i
        End If
    Next
    Return UBound(lines)
End Function

' Moves the block out of the .bas into a .frm of its own.
Public Sub MoveToFrm()
    If inFrm Then Exit Sub
    Dim lines() As String
    ReadLines basPath, lines
    Dim first As Integer = -1
    If UBound(lines) >= 0 Then first = BlockStart(lines)
    If first >= 0 Then
        Dim last As Integer = BlockEndLine(lines, first)
        Dim out As String = ""
        For i As Integer = 0 To UBound(lines)
            If i < first Or i > last Then out = out & lines(i) & vbLf
        Next
        WriteSafely basPath, out
    End If
    inFrm = True
    WriteSafely frmPath, BlockText()
    loadedText = ReadText(frmPath)
End Sub

' -- Event handlers ----------------------------------------------------------

' The line (from 1) of "Sub name" in the .bas, or 0.
Public Function HandlerLine(subName As String) As Integer
    Dim lines() As String
    ReadLines basPath, lines
    Return HandlerIn(lines, subName)
End Function

' The same in lines already read (to look for several at once).
Public Function HandlerIn(lines() As String, subName As String) As Integer
    For i As Integer = 0 To UBound(lines)
        Dim t As String = Trim$(lines(i))
        If LCase$(Left$(t, 8)) = "private " Then t = Trim$(Mid$(t, 9))
        If LCase$(Left$(t, 7)) = "public " Then t = Trim$(Mid$(t, 8))
        If LCase$(Left$(t, 4)) = "sub " Then
            Dim rest As String = Trim$(Mid$(t, 5))
            If rest = subName Or Left$(rest, Len(subName) + 1) = subName & "(" Or Left$(rest, Len(subName) + 1) = subName & " " Then
                Return i + 1
            End If
        End If
    Next
    Return 0
End Function

' Writes an empty handler at the end of the .bas (which it creates if
' need be); returns the line of its Sub.
Public Function AddHandler(subName As String, params As String) As Integer
    Dim text As String = ReadText(basPath)
    Dim lines() As String
    Dim n As Integer = ReadLines(basPath, lines)
    If text <> "" Then
        text = text & vbLf
        n = n + 1
    End If
    text = text & "Sub " & subName & "(" & params & ")" & vbLf & "End Sub" & vbLf
    WriteSafely basPath, text
    If Not inFrm Then loadedText = text
    Return n + 1
End Function
