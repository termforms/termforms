' viewer.bas -- the TermForms file viewer: a folder's files on the left,
' the chosen one's lines on the right, with its size and date.
'
' Build and run:   tfc viewer.bas && ./viewer [folder]
' Keys:            arrows choose a file; Tab moves to its lines; F5
'                  refreshes; Ctrl+O opens another folder; Ctrl+L turns
'                  line numbers on and off.
Begin Form frmViewer
    Caption = "File viewer"
    Width = 78
    Height = 22
    Begin Menu mnuFile
        Caption = "&File"
        Begin Menu mnuOpen
            Caption = "&Open folder..."
            Shortcut = "Ctrl+O"
        End
        Begin Menu mnuRefresh
            Caption = "&Refresh"
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
    Begin Menu mnuView
        Caption = "&View"
        Begin Menu mnuNumbers
            Caption = "&Line numbers"
            Shortcut = "Ctrl+L"
            Checked = True
        End
    End
    Begin Label lblFolder
        Left = 1
        Width = 74
    End
    Begin ListBox lstFiles
        Left = 1
        Top = 2
        Width = 26
        Height = 17
    End
    Begin Label lblInfo
        Left = 29
        Top = 1
        Width = 46
    End
    Begin ListBox lstLines
        Left = 29
        Top = 2
        Width = 47
        Height = 17
    End
End

' More lines than this are not read: the list would be slow to fill.
Const MostLines = 5000

Sub Form_Load()
    If Command$ <> "" Then
        OpenFolder Command$
    End If
    ShowFolder
End Sub

Sub OpenFolder(folder As String)
    Try
        ChDir folder
    Catch
        MsgBox "Cannot open " & folder & ": " & Err.Description, vbExclamation, "Open folder"
    End Try
End Sub

' The last part of a path: the folder's own name (/ or \ between parts).
Function LastPart(path As String) As String
    Dim at As Integer = InStrRev(path, "/")
    If InStrRev(path, "\") > at Then
        at = InStrRev(path, "\")
    End If
    Return Mid$(path, at + 1)
End Function

' The current folder's files, sorted (Dir$ gives them in order).
Sub ShowFolder()
    lblFolder.Caption = "Folder: " & LastPart(CurDir$) & "  (File > Open folder... for another)"
    lstFiles.Clear
    lstLines.Clear
    Dim name As String = Dir$("*")
    Do While name <> ""
        lstFiles.AddItem name
        name = Dir$()
    Loop
    If lstFiles.ListCount = 0 Then
        lblInfo.Caption = "(no files here)"
    Else
        lblInfo.Caption = lstFiles.ListCount & " files"
        lstFiles.ListIndex = 0
    End If
End Sub

Sub ShowFile(name As String)
    lstLines.Clear
    lblInfo.Caption = FileLen(name) & " bytes, " & FileDateTime(name)
    Dim n As Integer = FreeFile
    Try
        Open name For Input As #n
        Dim count As Integer = 0
        Do While Not EOF(n) And count < MostLines
            Dim text As String
            Line Input #n, text
            count = count + 1
            If mnuNumbers.Checked Then
                lstLines.AddItem Right$("    " & count, 5) & "  " & text
            Else
                lstLines.AddItem text
            End If
        Loop
        If Not EOF(n) Then
            lblInfo.Caption = lblInfo.Caption & " (first " & MostLines & " lines)"
        End If
    Catch
        lblInfo.Caption = "Cannot read " & name & ": " & Err.Description
    Finally
        Close #n
    End Try
End Sub

Sub lstFiles_Click()
    If lstFiles.ListIndex >= 0 Then
        ShowFile lstFiles.Text
    End If
End Sub

Sub mnuOpen_Click()
    Dim folder As String = InputBox("Folder to show:", "Open folder", CurDir$)
    If folder <> "" Then
        OpenFolder folder
        ShowFolder
    End If
End Sub

Sub mnuRefresh_Click()
    ShowFolder
End Sub

Sub mnuNumbers_Click()
    mnuNumbers.Checked = Not mnuNumbers.Checked
    lstFiles_Click
End Sub

Sub mnuExit_Click()
    Unload Me
End Sub
