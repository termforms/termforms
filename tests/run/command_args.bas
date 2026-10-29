' Run test: CommandArgs() and App (the test passes five arguments, see
' command_args.args: one with a space, an empty one, one with quotes, one
' with backslashes, one beyond ASCII). On Windows they come through the
' runtime's own splitting of the command line, so this checks it against
' the rules Python quotes by. App's folder differs by machine, so the
' test shows how its parts fit together rather than the path itself.
Begin Form f
    Caption = "Arguments"
    Width = 56
    Height = 15
    Begin ListBox lstArgs
        Width = 50
        Height = 12
    End
End

Sub Form_Load()
    Dim a() As String
    a = CommandArgs()
    lstArgs.AddItem CStr(UBound(a) + 1) & " arguments: [" & Command$ & "]"
    For Each s As String In CommandArgs
        lstArgs.AddItem "[" & s & "] " & Len(s)
    Next
    Dim sep As String = "/"
    If Platform = "Windows" Then sep = "\"
    Dim exe As String = App.EXEName
    If Platform = "Windows" Then exe = exe & ".exe"
    lstArgs.AddItem "EXEName " & App.EXEName
    lstArgs.AddItem "FullPath is Path + name: " & (App.FullPath = App.Path & sep & exe)
    lstArgs.AddItem "FullPath exists: " & Exists(App.FullPath)
    lstArgs.AddItem "Path has no end separator: " & (Right$(App.Path, 1) <> sep)
End Sub
