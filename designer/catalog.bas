' catalog.bas -- what tfc knows about controls, read from `tfc --catalog`
' once at the start, so the designer never offers a control, property or
' event the compiler does not know. The runtime's default values come
' from a FormView (frmDesigner fills them in): GetProperty of a control
' just added gives them.

Public kinds() As String                   ' every control type, in the catalog's order
Public kindCount As Integer
Private propList As Dictionary(Of String)   ' kind -> "Caption,Left,..."
Private typeList As Dictionary(Of String)    ' "kind.prop" -> String, Integer or Boolean
Private flagList As Dictionary(Of String)    ' "kind.prop" -> "", read-only or form-only
Private eventList As Dictionary(Of String)  ' kind -> "Click,GotFocus,..."
Private paramList As Dictionary(Of String)  ' "kind.event" -> "ByRef Cancel As Integer"
Public defaults As Dictionary(Of String)   ' "kind.prop" -> the runtime's default, as text

' The tfc to ask: $TFC if set, else the one beside the designer, else
' whichever the PATH finds.
Public Function TfcCommand() As String
    Dim named As String = Environ$("TFC")
    If named <> "" Then Return named
    Dim exe As String = "tfc"
    If Platform = "Windows" Then exe = "tfc.exe"
    If Exists(App.Path & "/" & exe) Then Return Quoted(App.Path & "/" & exe)
    Return "tfc"
End Function

Public Function Quoted(path As String) As String
    Return Chr$(34) & path & Chr$(34)
End Function

Public Function TempFolder() As String
    Dim t As String = Environ$("TMPDIR")
    If t = "" Then t = Environ$("TEMP")
    If t = "" Then t = "/tmp"
    Return t
End Function

' Reads the catalog; returns "" or why it could not.
Public Function LoadCatalog() As String
    Dim listing As String = TempFolder() & "/tfdesign-catalog-" & CStr(CLng(Timer * 1000)) & ".txt"
    Dim code As Integer = Shell(TfcCommand() & " --catalog > " & Quoted(listing))
    If code <> 0 Or Not Exists(listing) Then
        If Exists(listing) Then Kill listing
        Return "cannot run " & TfcCommand() & " --catalog (set TFC to tfc's path)"
    End If
    Dim kind As String = ""
    Dim n As Integer = FreeFile
    Open listing For Input As #n
    Do While Not EOF(n)
        Dim line As String
        Line Input #n, line
        Dim words() As String = Split(line, " ")
        If UBound(words) >= 1 Then
            Select Case words(0)
                Case "control"
                    kind = words(1)
                    ReDim Preserve kinds(kindCount)
                    kinds(kindCount) = kind
                    kindCount = kindCount + 1
                Case "property"
                    If propList.Exists(kind) Then
                        propList(kind) = propList(kind) & "," & words(1)
                    Else
                        propList(kind) = words(1)
                    End If
                    typeList(kind & "." & words(1)) = words(2)
                    flagList(kind & "." & words(1)) = ""
                    If UBound(words) >= 3 Then flagList(kind & "." & words(1)) = words(3)
                Case "event"
                    If eventList.Exists(kind) Then
                        eventList(kind) = eventList(kind) & "," & words(1)
                    Else
                        eventList(kind) = words(1)
                    End If
                    paramList(kind & "." & words(1)) = Trim$(Mid$(line, Len("event " & words(1)) + 1))
            End Select
        End If
    Loop
    Close #n
    Kill listing
    If kindCount = 0 Then Return "tfc --catalog listed no controls"
    Return ""
End Function

' Every property, in the catalog's order, joined with commas.
Public Function PropOrder(kind As String) As String
    If propList.Exists(kind) Then Return propList(kind)
    Return ""
End Function

Public Function PropType(kind As String, prop As String) As String
    If typeList.Exists(kind & "." & prop) Then Return typeList(kind & "." & prop)
    Return "String"
End Function

Public Function PropFlag(kind As String, prop As String) As String
    If flagList.Exists(kind & "." & prop) Then Return flagList(kind & "." & prop)
    Return ""
End Function

' Whether a form block may set it: not read-only, and not one that only
' means something while the program runs (the selection, the item chosen).
Public Function Designable(kind As String, prop As String) As Boolean
    If PropFlag(kind, prop) = "read-only" Then Return False
    Select Case prop
        Case "SelStart", "SelLength", "SelText", "ListIndex", "TopIndex"
            Return False
        Case "Text"
            Return kind = "TextBox" Or kind = "ComboBox"
    End Select
    Return True
End Function

Public Function EventOrder(kind As String) As String
    If eventList.Exists(kind) Then Return eventList(kind)
    Return ""
End Function

Public Function EventParams(kind As String, eventName As String) As String
    If paramList.Exists(kind & "." & eventName) Then Return paramList(kind & "." & eventName)
    Return ""
End Function

Public Function DefaultOf(kind As String, prop As String) As String
    If defaults.Exists(kind & "." & prop) Then Return defaults(kind & "." & prop)
    Return ""
End Function
