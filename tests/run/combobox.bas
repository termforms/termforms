' Run test: ComboBoxes. cboColour (Style 2, Sorted) is the list only:
' Down chooses the first item, then the next; a letter goes to the next
' item starting with it; F4 opens the list (DropDown first) and Esc
' closes it unchanged without pressing the Cancel button; Alt+Down opens
' it again, Up and Enter choose. Setting Text in code chooses the item
' with that text, or is error 380. cboCity (Style 0) has a text part:
' typing fires Change and leaves no item chosen; Down chooses the first
' item, putting its text in; a click on the arrow opens the list and a
' click on an item chooses it. Last, a click on cboColour opens its list
' (more items than rows, so a scroll bar), the wheel moves through it and
' Enter chooses; F4 opens it again, shown over the controls below.
Begin Form frmMain
    Caption = "Combo"
    Width = 50
    Height = 16
    Begin ComboBox cboColour
        Width = 16
        Style = 2
        Sorted = True
    End
    Begin ComboBox cboCity
        Left = 18
        Width = 16
    End
    Begin CommandButton cmdCancel
        Caption = "Cancel"
        Left = 36
        Width = 10
        Cancel = True
    End
    Begin Label lblLog
        Left = 17
        Top = 2
        Width = 31
        Height = 12
    End
End

Sub Note(s As String)
    lblLog.Caption = lblLog.Caption & s & " "
End Sub

Sub Form_Load()
    For Each c As String In Split("red green blue black white yellow orange purple brown grey", " ")
        cboColour.AddItem c
    Next
    cboCity.AddItem "Athens"
    cboCity.AddItem "Berlin"
    cboCity.AddItem "Cairo"
    Try
        cboColour.Text = "pink"
    Catch
        Note "E" & Err.Number
    End Try
    cboColour.Text = "red"
    cboColour.ListIndex = -1
    Note "n=" & cboColour.ListCount & " style=" & cboColour.Style & "/" & cboCity.Style & " |"
End Sub

Sub cboColour_Click()
    Note "C:" & cboColour.ListIndex & ":" & cboColour.Text
End Sub

Sub cboColour_DropDown()
    Note "D"
End Sub

Sub cboCity_Click()
    Note "c:" & cboCity.ListIndex & ":" & cboCity.Text
End Sub

Sub cboCity_Change()
    Note "ch:" & cboCity.Text & ":" & cboCity.ListIndex
End Sub

Sub cboCity_DropDown()
    Note "d"
End Sub

Sub cmdCancel_Click()
    Note "cancel!"
End Sub
