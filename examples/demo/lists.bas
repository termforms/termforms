' lists.bas -- ListBoxes: a Sorted one with items added and removed, and
' the two MultiSelect kinds (1: Space toggles; 2: arrows choose, Space or
' Ctrl+click adds). The mouse wheel scrolls them.
Begin Form frmLists
    Caption = "Lists"
    Width = 66
    Height = 18
    Begin Label lblFruit
        Caption = "&Fruit (sorted):"
        Left = 1
        Width = 18
    End
    Begin ListBox lstFruit
        Left = 1
        Top = 1
        Width = 20
        Height = 10
        Sorted = True
    End
    Begin Label lblMulti
        Caption = "&Toppings (1):"
        Left = 23
        Width = 18
    End
    Begin ListBox lstToppings
        Left = 23
        Top = 1
        Width = 18
        Height = 10
        MultiSelect = 1
    End
    Begin Label lblExtended
        Caption = "&Days (2):"
        Left = 43
        Width = 18
    End
    Begin ListBox lstDays
        Left = 43
        Top = 1
        Width = 20
        Height = 10
        MultiSelect = 2
    End
    Begin Label lblNew
        Caption = "&New fruit:"
        Left = 1
        Top = 12
        Width = 11
    End
    Begin TextBox txtNew
        Left = 12
        Top = 12
        Width = 14
    End
    Begin CommandButton cmdAdd
        Caption = "&Add"
        Left = 28
        Top = 12
        Width = 10
        Default = True
    End
    Begin CommandButton cmdRemove
        Caption = "&Remove"
        Left = 40
        Top = 12
        Width = 12
    End
    Begin CommandButton cmdClose
        Caption = "Close"
        Left = 54
        Top = 12
        Width = 10
        Cancel = True
    End
    Begin Label lblCount
        Left = 1
        Top = 14
        Width = 62
    End
End

Sub Form_Load()
    For Each f As String In Split("pear apple kiwi banana fig mango cherry plum")
        lstFruit.AddItem f
    Next
    For Each t As String In Split("cheese olives ham basil mushroom onion peppers anchovies egg")
        lstToppings.AddItem t
    Next
    Dim days() As String = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"}
    For Each d As String In days
        lstDays.AddItem d
    Next
    Note "frmLists Load: " & lstFruit.ListCount & " fruit"
    ShowCounts
End Sub

Sub ShowCounts()
    lblCount.Caption = "fruit " & lstFruit.ListCount & ", toppings chosen " & lstToppings.SelCount & _
                       ", days chosen " & lstDays.SelCount
End Sub

Sub lstFruit_Click()
    Note "lstFruit Click: " & lstFruit.Text
End Sub

Sub lstFruit_DblClick()
    MsgBox "You chose " & lstFruit.Text & ".", vbInformation, "Fruit"
End Sub

Sub lstToppings_Click()
    Note "lstToppings Click: " & lstToppings.SelCount & " chosen"
    ShowCounts
End Sub

Sub lstDays_Click()
    Note "lstDays Click: " & lstDays.Text
    ShowCounts
End Sub

Sub cmdAdd_Click()
    If txtNew.Text = "" Then
        Note "Add: nothing to add"
        Exit Sub
    End If
    lstFruit.AddItem txtNew.Text
    Note "Add: " & txtNew.Text
    txtNew.Text = ""
    ShowCounts
End Sub

Sub cmdRemove_Click()
    If lstFruit.ListIndex < 0 Then
        MsgBox "Choose a fruit first.", vbExclamation, "Remove"
        Exit Sub
    End If
    Note "Remove: " & lstFruit.Text
    lstFruit.RemoveItem lstFruit.ListIndex
    ShowCounts
End Sub

Sub cmdClose_Click()
    Unload Me
End Sub
