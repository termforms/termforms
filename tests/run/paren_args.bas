' Run test: a statement call whose first argument starts with a
' parenthesis, as VB6 reads it: the parenthesis belongs to that argument
' when more of the statement follows it (lst.AddItem (n + 1) & " items"),
' and is the argument list when nothing does (lst.AddItem ("spaced")).
' Methods, Dictionary's Add and Subs alike.
Begin Form f
    Caption = "Paren"
    Width = 40
    Height = 11
    Begin ListBox lst
        Width = 36
        Height = 8
    End
End
Dim d As Dictionary(Of Integer)
Sub Note(s As String)
    lst.AddItem s
End Sub
Sub Form_Load()
    Dim n As Integer = 2
    lst.AddItem (n + 1) & " items"
    lst.AddItem ("first"), 0
    lst.AddItem("plain")
    lst.AddItem ("spaced")
    Note (n * 2) & " via a Sub"
    d.Add ("k" & n), n
    lst.AddItem "key k2: " & d("k2")
End Sub
