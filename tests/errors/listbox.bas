' Errors test: ListBoxes: a read-only property set (in the form block and
' in code), a method the ListBox lacks, a ListBox method on another
' control, a number where an item's text is wanted, and List(i) of a
' String index.
Begin Form f
    Begin ListBox lst
        ListCount = 3
    End
    Begin CommandButton b
    End
End

Sub b_Click()
    Dim s As String
    lst.ListCount = 1
    lst.Sort
    b.AddItem "x"
    lst.AddItem 5
    s = lst.List("a")
End Sub
