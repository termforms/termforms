' conflicts.bas -- the resolver's work: Syncthing's folders and devices
' from its config.xml, the conflict copies found in each folder, and the
' three ways to resolve one.
'
' Syncthing names the copy it set aside
'     name.sync-conflict-YYYYMMDD-HHMMSS-DEVICE7.ext
' (the time in UTC, DEVICE7 the first 7 characters of the ID of the
' device whose version lost). Nothing here deletes: a copy not kept is
' moved to the removed folder, outside every synced folder, keeping its
' path, so a wrong choice can be undone by hand.

Type SyncFolder
    Label As String
    Path As String ' with no / at the end
    Count As Integer ' its conflicts not yet resolved
End Type

Type Conflict
    Folder As Integer ' in folders()
    Dir As String ' its folder inside the synced one: "" or "notes/"
    Name As String ' the conflict copy's name
    Original As String ' the name it conflicts with, in the same folder
    Device As String ' 7 characters
    Stamp As String ' YYYYMMDD-HHMMSS, UTC
    Done As Boolean
End Type

Public Const Marker = ".sync-conflict-"

Public folders() As SyncFolder
Public items() As Conflict
Public deviceNames As Dictionary(Of String) ' by the 7-character ID
Public configFile As String ' "" when none was found
Public removedRoot As String
Public skipped As Integer ' folders that could not be read in the last scan

' ---- Command line and config.xml ----

' syncfix [--config FILE] [--removed FOLDER] [FOLDER...]: folders given
' are used instead of config.xml's (which still names the devices).
Sub ReadArguments()
    Dim args() As String = CommandArgs()
    Dim given() As String
    Dim i As Integer = 0
    Do While i <= UBound(args)
        If args(i) = "--config" And i < UBound(args) Then
            configFile = args(i + 1)
            i = i + 1
        ElseIf args(i) = "--removed" And i < UBound(args) Then
            removedRoot = WithoutSlash(args(i + 1))
            i = i + 1
        Else
            ReDim Preserve given(UBound(given) + 1)
            given(UBound(given)) = WithoutSlash(args(i))
        End If
        i = i + 1
    Loop
    If configFile = "" Then
        configFile = FindConfig()
    End If
    If removedRoot = "" Then
        removedRoot = Home() & "/sync-conflicts-removed"
    End If
    If configFile <> "" Then
        ReadConfig UBound(given) < 0
    End If
    For Each path As String In given
        AddFolder LastPart(path), path
    Next
End Sub

Function Home() As String
    If Platform = "Windows" Then
        Return Replace(Environ$("USERPROFILE"), "\", "/")
    End If
    Return Environ$("HOME")
End Function

' Where Syncthing keeps config.xml for the user running this program;
' STHOMEDIR first, as Syncthing itself does.
Function FindConfig() As String
    Dim own As String = Environ$("STHOMEDIR")
    If own <> "" And Exists(own & "/config.xml") Then
        Return own & "/config.xml"
    End If
    Dim h As String = Home()
    Dim candidates() As String = {h & "/.local/state/syncthing/config.xml", h & "/.config/syncthing/config.xml", h & "/Library/Application Support/Syncthing/config.xml"}
    If Platform = "Windows" Then
        candidates = Split(Replace(Environ$("LOCALAPPDATA"), "\", "/") & "/Syncthing/config.xml", vbLf)
    End If
    For Each c As String In candidates
        If Exists(c) Then
            Return c
        End If
    Next
    Return ""
End Function

' config.xml has one element a line. The folders' own <device> lines have
' no name, and the <defaults> section's folder has no id, so both are
' passed over.
Sub ReadConfig(takeFolders As Boolean)
    Dim n As Integer = FreeFile
    Try
        Open configFile For Input As #n
        Do While Not EOF(n)
            Dim line As String
            Line Input #n, line
            line = Trim$(line)
            If takeFolders And Left$(line, 8) = "<folder " Then
                Dim id As String = Attribute(line, "id")
                Dim path As String = Attribute(line, "path")
                If id <> "" And path <> "" Then
                    Dim label As String = Attribute(line, "label")
                    If label = "" Then
                        label = id
                    End If
                    AddFolder label, ExpandHome(path)
                End If
            ElseIf Left$(line, 8) = "<device " Then
                Dim device As String = Attribute(line, "id")
                Dim name As String = Attribute(line, "name")
                If Len(device) >= 7 And name <> "" Then
                    deviceNames(Left$(device, 7)) = name
                End If
            End If
        Loop
    Catch
        configFile = ""
    Finally
        Close #n
    End Try
End Sub

' The value of name="..." in an XML start tag, entities decoded.
Function Attribute(tag As String, name As String) As String
    Dim at As Integer = InStr(tag, " " & name & "=""")
    If at = 0 Then
        Return ""
    End If
    at = at + Len(name) + 3
    Dim ending As Integer = InStr(at, tag, """")
    If ending = 0 Then
        Return ""
    End If
    Dim value As String = Mid$(tag, at, ending - at)
    value = Replace(value, "&lt;", "<")
    value = Replace(value, "&gt;", ">")
    value = Replace(value, "&quot;", """")
    value = Replace(value, "&apos;", "'")
    Return Replace(value, "&amp;", "&")
End Function

Function ExpandHome(path As String) As String
    If path = "~" Or Left$(path, 2) = "~/" Or Left$(path, 2) = "~\" Then
        Return Home() & Mid$(path, 2)
    End If
    Return path
End Function

Sub AddFolder(label As String, path As String)
    ReDim Preserve folders(UBound(folders) + 1)
    folders(UBound(folders)).Label = label
    folders(UBound(folders)).Path = WithoutSlash(Replace(path, "\", "/"))
End Sub

Function WithoutSlash(path As String) As String
    If Len(path) > 1 And (Right$(path, 1) = "/" Or Right$(path, 1) = "\") Then
        Return Left$(path, Len(path) - 1)
    End If
    Return path
End Function

Function LastPart(path As String) As String
    Return Mid$(path, InStrRev(path, "/") + 1)
End Function

' The synced folder holding the removed folder, if one does: copies moved
' there would sync again.
Function RemovedInside() As String
    For Each f As SyncFolder In folders
        If removedRoot = f.Path Or Left$(removedRoot, Len(f.Path) + 1) = f.Path & "/" Then
            Return f.Label
        End If
    Next
    Return ""
End Function

' ---- Finding conflicts ----

Sub Scan()
    Erase items
    skipped = 0
    For i As Integer = 0 To UBound(folders)
        folders(i).Count = 0
        Walk i, ""
    Next
End Sub

' Syncthing's own folders are passed over: .stversions keeps old copies
' under their conflict names, which are not conflicts any more.
Sub Walk(f As Integer, dir As String)
    Dim here As String = folders(f).Path & "/" & dir
    Dim names() As String
    Dim subs() As String
    Try
        names = Files(here & "*")
        subs = Folders(here & "*")
    Catch
        skipped = skipped + 1
        Exit Sub
    End Try
    For Each name As String In names
        If InStr(name, Marker) > 0 Then
            AddConflict f, dir, name
        End If
    Next
    For Each s As String In subs
        If s <> ".stversions" And s <> ".stfolder" Then
            Walk f, dir & s & "/"
        End If
    Next
End Sub

' A name with the marker but not Syncthing's form after it is left alone.
Sub AddConflict(f As Integer, dir As String, name As String)
    Dim at As Integer = InStrRev(name, Marker)
    Dim tail As String = Mid$(name, at + Len(Marker))
    If Len(tail) < 23 Or Mid$(tail, 9, 1) <> "-" Or Mid$(tail, 16, 1) <> "-" Then
        Exit Sub
    End If
    If Not Digits(Left$(tail, 8)) Or Not Digits(Mid$(tail, 10, 6)) Then
        Exit Sub
    End If
    Dim c As Conflict
    c.Folder = f
    c.Dir = dir
    c.Name = name
    c.Stamp = Left$(tail, 15)
    c.Device = Mid$(tail, 17, 7)
    c.Original = Left$(name, at - 1) & Mid$(tail, 24)
    ReDim Preserve items(UBound(items) + 1)
    items(UBound(items)) = c
    folders(f).Count = folders(f).Count + 1
End Sub

Function Digits(text As String) As Boolean
    For i As Integer = 1 To Len(text)
        If InStr("0123456789", Mid$(text, i, 1)) = 0 Then
            Return False
        End If
    Next
    Return True
End Function

' ---- Describing one ----

Function FullPath(c As Conflict, name As String) As String
    Return folders(c.Folder).Path & "/" & c.Dir & name
End Function

Function DeviceName(c As Conflict) As String
    If deviceNames.Exists(c.Device) Then
        Return deviceNames(c.Device)
    End If
    Return c.Device
End Function

' Unknown only when config.xml was read: without it no device is known.
Function UnknownDevice(c As Conflict) As Boolean
    Return configFile <> "" And Not deviceNames.Exists(c.Device)
End Function

' "2026-10-28 09:12 UTC" from the name's stamp.
Function WhenSetAside(c As Conflict) As String
    Return Left$(c.Stamp, 4) & "-" & Mid$(c.Stamp, 5, 2) & "-" & Mid$(c.Stamp, 7, 2) & " " & Mid$(c.Stamp, 10, 2) & ":" & Mid$(c.Stamp, 12, 2) & " UTC"
End Function

' ---- Resolving ----

' The original stays; the conflict copy moves out.
Function KeepOriginal(c As Conflict) As String
    Return c.Dir & c.Name & " moved to " & MoveOut(c, c.Name)
End Function

' The conflict copy takes the original's name (the original moved out
' first, as Name never replaces a file), so Syncthing sends it everywhere.
' If the rename fails, the original is put back.
Function KeepConflict(c As Conflict) As String
    Dim report As String = ""
    Dim moved As String = ""
    If Exists(FullPath(c, c.Original)) Then
        moved = MoveOut(c, c.Original)
        report = c.Dir & c.Original & " moved to " & moved & "; "
    End If
    Try
        Name FullPath(c, c.Name) As FullPath(c, c.Original)
    Catch When moved <> ""
        Dim number As Integer = Err.Number
        Dim description As String = Err.Description
        Name moved As FullPath(c, c.Original)
        Throw number, description
    End Try
    Return report & c.Dir & c.Name & " renamed " & c.Original
End Function

' Both stay: the conflict copy gets an ordinary name, so the warning
' clears and the decision can wait.
Function KeepBoth(c As Conflict) As String
    Dim kept As String = Unique(folders(c.Folder).Path & "/" & c.Dir, Suffixed(c.Original, " (" & DeviceName(c) & " " & Left$(WhenSetAside(c), 10) & ")"))
    Name FullPath(c, c.Name) As FullPath(c, kept)
    Return c.Dir & c.Name & " renamed " & kept
End Function

' name with extra before its extension: todo.txt -> todo (tablet).txt.
Function Suffixed(name As String, extra As String) As String
    Dim dot As Integer = InStrRev(name, ".")
    If dot <= 1 Then
        Return name & extra
    End If
    Return Left$(name, dot - 1) & extra & Mid$(name, dot)
End Function

' name, or name (2), name (3)... if dir already has it.
Function Unique(dir As String, name As String) As String
    Dim n As Integer = 1
    Dim tried As String = name
    Do While Exists(dir & tried)
        n = n + 1
        tried = Suffixed(name, " (" & n & ")")
    Loop
    Return tried
End Function

' Today's folder in the removed folder, made if need be.
Function RemovedToday() As String
    Dim today As String = removedRoot & "/" & Left$(Now, 10)
    MakeFolders today
    Return today
End Function

' Moves one of c's two files to the removed folder, under the synced
' folder's label and its own path there, and gives where it went. Name
' fails across file systems (75): then it is copied and deleted, which
' loses its date.
Function MoveOut(c As Conflict, name As String) As String
    Dim label As String = Replace(Replace(folders(c.Folder).Label, "/", "_"), "\", "_")
    Dim dir As String = RemovedToday() & "/" & label & "/" & c.Dir
    MakeFolders WithoutSlash(dir)
    Dim target As String = dir & Unique(dir, name)
    Try
        Name FullPath(c, name) As target
    Catch When Err.Number = 75
        FileCopy FullPath(c, name), target
        Kill FullPath(c, name)
    End Try
    Return target
End Function

' MkDir for each missing folder in path.
Sub MakeFolders(path As String)
    If path = "" Or Exists(path) Then
        Exit Sub
    End If
    Dim at As Integer = InStrRev(path, "/")
    If at > 1 Then
        MakeFolders Left$(path, at - 1)
    End If
    MkDir path
End Sub

' A line in today's log.txt in the removed folder.
Sub Note(text As String)
    Dim n As Integer = FreeFile
    Open RemovedToday() & "/log.txt" For Append As #n
    Print #n, Now; "  "; text
    Close #n
End Sub

' Single quotes for sh: ' itself becomes '\''.
Function Quoted(path As String) As String
    If Platform = "Windows" Then
        Return """" & path & """"
    End If
    Return "'" & Replace(path, "'", "'\''") & "'"
End Function

' The command showing the two versions' differences, or the conflict
' copy alone when the original is not here.
Function CompareCommand(c As Conflict) As String
    Dim original As String = Quoted(FullPath(c, c.Original))
    Dim copy As String = Quoted(FullPath(c, c.Name))
    If Platform = "Windows" Then
        If Exists(FullPath(c, c.Original)) Then
            Return "fc /n " & original & " " & copy & " | more"
        End If
        Return "more < " & copy
    End If
    If Exists(FullPath(c, c.Original)) Then
        Return "diff -u -- " & original & " " & copy & " | ${PAGER:-less}"
    End If
    Return "${PAGER:-less} -- " & copy
End Function
