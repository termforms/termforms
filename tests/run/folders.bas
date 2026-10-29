' Run test: Files(), Folders() and IsLink: sorted names, dot names
' included, wildcards, a link to a folder in Files() but not Folders()
' (ln -s, or a junction on Windows), a recursive walk (each call its own
' listing, unlike Dir$), no match as an empty array, and error 76 for a
' missing folder, caught.
Begin Form f
    Caption = "Folders"
    Width = 72
    Height = 10
    Begin TextBox t1
        Width = 66
    End
    Begin TextBox t2
        Top = 1
        Width = 66
    End
    Begin TextBox t3
        Top = 2
        Width = 66
    End
    Begin TextBox t4
        Top = 3
        Width = 66
    End
    Begin TextBox t5
        Top = 4
        Width = 66
    End
End

Dim walked As String

Sub Touch(path As String)
    Open path For Output As #1
    Close #1
End Sub

Sub Walk(path As String)
    For Each name As String In Files(path & "/*")
        walked = walked & path & "/" & name & " "
    Next
    For Each name As String In Folders(path & "/*")
        Walk path & "/" & name
    Next
End Sub

Function Joined(names() As String) As String
    Joined = UBound(names) & ":" & Join(names, ",")
End Function

Sub Form_Load()
    Dim names() As String
    Dim s As String
    MkDir "box"
    MkDir "box/sub"
    MkDir "box/sub/deep"
    Touch "box/b.txt"
    Touch "box/a.txt"
    Touch "box/.hidden"
    Touch "box/sub/x.txt"
    Touch "box/sub/deep/y.txt"
    If Platform = "Windows" Then
        Shell "mklink /J box\link """ & CurDir$ & "\box\sub"" > NUL"
    Else
        Shell "ln -s sub box/link"
    End If

    names = Files("box/*")
    s = Joined(names)
    names = Folders("box/*")
    t1.Text = s & " | " & Joined(names)

    names = Files("box/*.txt")
    s = Joined(names)
    names = Folders("box")
    s = s & " | " & Joined(names)
    names = Files("box/*.zzz")
    t2.Text = s & " | " & Joined(names)

    t3.Text = IsLink("box/link") & " " & IsLink("box/sub") & " " & IsLink("box/a.txt") & _
        " " & IsLink("nope") & " " & IsLink("")

    Walk "box"
    t4.Text = Replace(walked, "box/", "")

    Try
        names = Files("nope/*")
    Catch
        s = "nope " & Err.Number
    End Try
    Try
        names = Folders("box/a.txt/*")
    Catch
        s = s & ", file " & Err.Number
    End Try
    t5.Text = s
End Sub
