' Run test: a ListBox's methods and indexed properties inside With (.AddItem,
' .Selected(i) = b, .List(i) = s), which did not compile before 1.1.
Begin Form f
    Caption = "W"
    Width = 30
    Height = 8
    Begin ListBox lst
        Width = 20
        Height = 5
    End
End
Sub Form_Load()
    With lst
        .AddItem "one"
        .AddItem "two"
        .Selected(1) = True
        .List(0) = "uno"
    End With
End Sub
