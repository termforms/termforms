' Errors test: a ComboBox's Style is 0 or 2 (Style 1 is not provided) and
' is set in the form block only; Selected(i) is a ListBox's alone; a
' ComboBox's statements are AddItem, RemoveItem and Clear.
Begin Form f
    Begin ComboBox cboA
        Style = 1
    End
End

Sub Form_Load()
    cboA.Style = 2
    cboA.Selected(0) = True
    cboA.Sort
    cboA.AddItem "fine"
    cboA.List(0) = "also fine"
End Sub
