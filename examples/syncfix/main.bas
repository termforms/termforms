' main.bas -- the resolver's form: Syncthing's folders on the left, the
' chosen one's conflicts on the right, the two versions below, and the
' three ways to resolve them.
Begin Form frmMain
    Caption = "Syncthing conflicts"
    Width = 80
    Height = 24
    Begin Menu mnuFile
        Caption = "&File"
        Begin Menu mnuRescan
            Caption = "&Rescan"
            Shortcut = "F5"
        End
        Begin Menu mnuFileSep
            Caption = "-"
        End
        Begin Menu mnuExit
            Caption = "E&xit"
            Shortcut = "Ctrl+Q"
        End
    End
    Begin Menu mnuHelp
        Caption = "&Help"
        Begin Menu mnuKeys
            Caption = "&Keys and choices..."
            Shortcut = "F1"
        End
        Begin Menu mnuWhere
            Caption = "&Where things are..."
        End
    End
    Begin Label lblFolders
        Caption = "Fol&ders"
        Left = 1
        Width = 20
    End
    Begin ListBox lstFolders
        Left = 1
        Top = 1
        Width = 20
        Height = 11
    End
    Begin Label lblConflicts
        Caption = "Co&nflicts"
        Left = 22
        Width = 55
    End
    Begin ListBox lstConflicts
        Left = 22
        Top = 1
        Width = 55
        Height = 11
        MultiSelect = 2
    End
    Begin Label lblOriginal
        Left = 1
        Top = 12
        Width = 76
    End
    Begin Label lblCopy
        Left = 1
        Top = 13
        Width = 76
    End
    Begin Label lblFrom
        Left = 1
        Top = 14
        Width = 76
        Height = 2
    End
    Begin CommandButton cmdOriginal
        Caption = "Keep &original"
        Left = 1
        Top = 16
        Width = 17
    End
    Begin CommandButton cmdConflict
        Caption = "Keep &conflict"
        Left = 19
        Top = 16
        Width = 17
    End
    Begin CommandButton cmdBoth
        Caption = "Keep &both"
        Left = 37
        Top = 16
        Width = 14
    End
    Begin CommandButton cmdCompare
        Caption = "Co&mpare"
        Left = 52
        Top = 16
        Width = 12
    End
    Begin CommandButton cmdRescan
        Caption = "&Rescan"
        Left = 65
        Top = 16
        Width = 12
    End
    Begin Label lblStatus
        Left = 1
        Top = 18
        Width = 76
        Height = 2
    End
End

' The items() index of each row in lstConflicts.
Dim shown() As Integer

Enum Choice
    Original
    Conflict
    Both
End Enum

Sub Form_Load()
    ReadArguments
    If UBound(folders) < 0 Then
        lblStatus.Caption = "No Syncthing config.xml found, so no folders. Give its place with --config FILE, or the folders to search as arguments."
        Exit Sub
    End If
    Rescan
    If RemovedInside() <> "" Then
        lblStatus.Caption = Plain("The removed folder " & removedRoot & " is inside " & RemovedInside() & ": nothing will be resolved. Give another with --removed FOLDER.")
    End If
End Sub

Sub Rescan()
    Scan
    ShowFolders
    ' The first folder with conflicts, else the first.
    lstFolders.ListIndex = 0
    For i As Integer = 0 To UBound(folders)
        If folders(i).Count > 0 Then
            lstFolders.ListIndex = i
            Exit For
        End If
    Next
    Dim report As String = (UBound(items) + 1) & " conflicts in " & (UBound(folders) + 1) & " folders."
    If skipped > 0 Then
        report = report & " " & skipped & " folders could not be read."
    End If
    If configFile = "" Then
        report = report & " No config.xml: devices are shown by ID."
    End If
    lblStatus.Caption = Plain(report)
End Sub

' Each folder with its count, keeping the one chosen.
Sub ShowFolders()
    Dim chosen As Integer = lstFolders.ListIndex
    lstFolders.Clear
    For Each f As SyncFolder In folders
        lstFolders.AddItem Fit(f.Label, 13) & Right$(Space$(5) & f.Count, 5)
    Next
    If chosen >= 0 And chosen < lstFolders.ListCount Then
        lstFolders.ListIndex = chosen
    End If
End Sub

' The chosen folder's conflicts not yet resolved, the row at near chosen.
Sub ShowConflicts(near As Integer)
    lstConflicts.Clear
    Erase shown
    Dim f As Integer = lstFolders.ListIndex
    If f < 0 Then
        Exit Sub
    End If
    For i As Integer = 0 To UBound(items)
        If items(i).Folder = f And Not items(i).Done Then
            ReDim Preserve shown(UBound(shown) + 1)
            shown(UBound(shown)) = i
            lstConflicts.AddItem Row(items(i))
        End If
    Next
    lblConflicts.Caption = "Co&nflicts in " & Plain(folders(f).Label) & " (" & lstConflicts.ListCount & ")"
    If lstConflicts.ListCount = 0 Then
        ShowDetails -1
    ElseIf near < lstConflicts.ListCount Then
        lstConflicts.ListIndex = near
    Else
        lstConflicts.ListIndex = lstConflicts.ListCount - 1
    End If
End Sub

' The original's path, the day it was set aside and the device: 53 cells.
Function Row(c As Conflict) As String
    Return Fit(c.Dir & c.Original, 31) & " " & Left$(WhenSetAside(c), 10) & " " & Left$(DeviceName(c), 10)
End Function

' A Label shows & as an access key's mark; && is one &.
Function Plain(text As String) As String
    Return Replace(text, "&", "&&")
End Function

' text padded to width, or its end with "…" before it if it is longer.
Function Fit(text As String, width As Integer) As String
    If Len(text) > width Then
        Return "…" & Right$(text, width - 1)
    End If
    Return text & Space$(width - Len(text))
End Function

Sub ShowDetails(row As Integer)
    If row < 0 Then
        lblOriginal.Caption = ""
        lblCopy.Caption = ""
        lblFrom.Caption = ""
        Exit Sub
    End If
    Dim c As Conflict = items(shown(row))
    lblOriginal.Caption = Plain("Original  " & Describe(c, c.Original))
    lblCopy.Caption = Plain("Conflict  " & Describe(c, c.Name))
    Dim from As String = "Set aside " & WhenSetAside(c) & ", the version from " & DeviceName(c)
    If UnknownDevice(c) Then
        from = from & ": not a device this machine knows. Find out which it is first."
    ElseIf DeviceName(c) <> c.Device Then
        from = from & " (" & c.Device & ")"
    End If
    lblFrom.Caption = Plain(from)
End Sub

' A file's path, size and date on one line of 66 cells.
Function Describe(c As Conflict, name As String) As String
    Dim path As String = FullPath(c, name)
    If Not Exists(path) Then
        Return Fit(c.Dir & name, 34) & "  not on this machine (ignored?)"
    End If
    Return Fit(c.Dir & name, 34) & Right$(Space$(14) & FileLen(path) & " bytes", 14) & "  " & Left$(FileDateTime(path), 16)
End Function

Sub lstFolders_Click()
    ShowConflicts 0
End Sub

Sub lstConflicts_Click()
    ShowDetails lstConflicts.ListIndex
End Sub

' The rows chosen: those selected, else the current one.
Sub Choose(ByRef rows() As Integer)
    Erase rows
    For i As Integer = 0 To lstConflicts.ListCount - 1
        If lstConflicts.Selected(i) Then
            ReDim Preserve rows(UBound(rows) + 1)
            rows(UBound(rows)) = i
        End If
    Next
    If UBound(rows) < 0 And lstConflicts.ListIndex >= 0 Then
        ReDim rows(0)
        rows(0) = lstConflicts.ListIndex
    End If
End Sub

Sub Resolve(how As Choice)
    If RemovedInside() <> "" Then
        MsgBox "The removed folder is inside a synced folder, so nothing is resolved.", vbExclamation, "Resolve"
        Exit Sub
    End If
    Dim rows() As Integer
    Choose rows
    If UBound(rows) < 0 Then
        lblStatus.Caption = "No conflict chosen."
        Exit Sub
    End If
    If UBound(rows) > 0 Then
        Dim what As String = "the originals"
        If how = Choice.Conflict Then
            what = "the conflict copies"
        ElseIf how = Choice.Both Then
            what = "both versions"
        End If
        If MsgBox("Keep " & what & " of " & (UBound(rows) + 1) & " files?", vbYesNo + vbQuestion, "Resolve") <> vbYes Then
            Exit Sub
        End If
    End If
    For Each r As Integer In rows
        Dim c As Conflict = items(shown(r))
        If UnknownDevice(c) Then
            If MsgBox(c.Device & " is not a device this machine knows. Find out which device it is before resolving its conflicts." & vbLf & vbLf & "Resolve anyway?", vbYesNo + vbExclamation + vbDefaultButton2, "Unknown device") <> vbYes Then
                Exit Sub
            End If
            Exit For
        End If
    Next
    Dim done As Integer = 0
    Dim report As String = ""
    For Each r As Integer In rows
        Dim k As Integer = shown(r)
        Try
            Dim c As Conflict = items(k)
            Dim what As String
            Select Case how
                Case Choice.Original
                    what = "Kept the original: " & KeepOriginal(c)
                    report = "Kept the original of " & c.Dir & c.Original & "."
                Case Choice.Conflict
                    what = "Kept the conflict: " & KeepConflict(c)
                    report = "Kept the conflict copy of " & c.Dir & c.Original & "."
                Case Else
                    what = "Kept both: " & KeepBoth(c)
                    report = "Kept both versions of " & c.Dir & c.Original & "."
            End Select
            items(k).Done = True
            folders(c.Folder).Count = folders(c.Folder).Count - 1
            done = done + 1
            Note what
        Catch
            report = "Could not resolve " & items(k).Dir & items(k).Name & ": " & Err.Description
            Exit For
        End Try
    Next
    If UBound(rows) > 0 Then
        report = done & " of " & (UBound(rows) + 1) & " resolved. " & report
    End If
    lblStatus.Caption = Plain(report)
    ShowFolders
    ShowConflicts rows(0)
End Sub

Sub cmdOriginal_Click()
    Resolve Choice.Original
End Sub

Sub cmdConflict_Click()
    Resolve Choice.Conflict
End Sub

Sub cmdBoth_Click()
    Resolve Choice.Both
End Sub

' diff through the pager; Shell hands the terminal over and back.
Sub cmdCompare_Click()
    If lstConflicts.ListIndex < 0 Then
        lblStatus.Caption = "No conflict chosen."
        Exit Sub
    End If
    Dim code As Integer = Shell(CompareCommand(items(shown(lstConflicts.ListIndex))))
    If code = 127 Then
        lblStatus.Caption = "diff or less is not installed here."
    End If
End Sub

Sub cmdRescan_Click()
    Rescan
End Sub

Sub mnuRescan_Click()
    Rescan
End Sub

Sub mnuExit_Click()
    Unload Me
End Sub

Sub mnuKeys_Click()
    MsgBox "Tab moves between the lists and buttons. In Conflicts, the arrows choose one; Space or Ctrl+click adds others, to resolve together." & vbLf & vbLf & "Keep original: the conflict copy moves to the removed folder." & vbLf & "Keep conflict: the original moves there, and the copy takes its name." & vbLf & "Keep both: the copy is renamed, e.g. todo (tablet 2026-10-28).txt." & vbLf & "Compare: the differences, in diff -u form.", vbInformation, "Keys and choices"
End Sub

Sub mnuWhere_Click()
    Dim config As String = configFile
    If config = "" Then
        config = "(none found)"
    End If
    MsgBox "config.xml: " & config & vbLf & "Removed copies and log.txt: " & removedRoot & "/<date>/", vbInformation, "Where things are"
End Sub
