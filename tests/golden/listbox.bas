' Golden test: a ListBox's methods and indexed properties as library calls
' (AddItem with and without an index, RemoveItem, Clear, List(i) read and
' set, Selected(i) read and set) and its properties.
Begin Form frmMain
    Caption = "Lists"
    Begin ListBox lst
        Height = 6
        MultiSelect = 1
    End
End

Sub Form_Load()
    lst.AddItem "a"
    lst.AddItem "b", 0
    lst.List(1) = lst.List(0) & "!"
    lst.Selected(1) = Not lst.Selected(1)
    If lst.ListCount > 1 Then lst.RemoveItem 0
    lst.ListIndex = 0
    lst.Clear
End Sub
