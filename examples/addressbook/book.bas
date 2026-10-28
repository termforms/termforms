' book.bas -- the address book's form: the names on the left (filtered by
' Find as you type), the chosen one's details on the right.
Begin Form frmBook
    Caption = "Address book"
    Width = 70
    Height = 20
    Begin Menu mnuFile
        Caption = "&File"
        Begin Menu mnuSave
            Caption = "&Save"
            Shortcut = "Ctrl+S"
        End
        Begin Menu mnuFileSep
            Caption = "-"
        End
        Begin Menu mnuExit
            Caption = "E&xit"
            Shortcut = "Ctrl+Q"
        End
    End
    Begin Menu mnuContact
        Caption = "&Contact"
        Begin Menu mnuNew
            Caption = "&New"
            Shortcut = "Ctrl+N"
        End
        Begin Menu mnuDelete
            Caption = "&Delete..."
        End
    End
    Begin Label lblFind
        Caption = "Fin&d:"
        Left = 1
        Width = 6
    End
    Begin TextBox txtFind
        Left = 7
        Width = 18
    End
    Begin ListBox lstNames
        Left = 1
        Top = 2
        Width = 24
        Height = 14
        Sorted = True
    End
    Begin Label lblName
        Caption = "&Name:"
        Left = 28
        Top = 2
        Width = 8
    End
    Begin TextBox txtName
        Left = 36
        Top = 2
        Width = 30
    End
    Begin Label lblPhone
        Caption = "&Phone:"
        Left = 28
        Top = 4
        Width = 8
    End
    Begin TextBox txtPhone
        Left = 36
        Top = 4
        Width = 30
    End
    Begin Label lblEmail
        Caption = "&Email:"
        Left = 28
        Top = 6
        Width = 8
    End
    Begin TextBox txtEmail
        Left = 36
        Top = 6
        Width = 30
    End
    Begin CommandButton cmdStore
        Caption = "S&tore"
        Left = 36
        Top = 8
        Width = 12
        Default = True
    End
    Begin CommandButton cmdClear
        Caption = "C&lear"
        Left = 50
        Top = 8
        Width = 12
    End
    Begin Label lblStatus
        Left = 28
        Top = 12
        Width = 38
        Height = 3
    End
End

Sub Form_Load()
    LoadContacts
    ShowNames
    lblStatus.Caption = contacts.Count & " contacts. Store keeps changes; Ctrl+S saves the file."
End Sub

Sub Form_Unload(ByRef Cancel As Integer)
    If Not changed Then
        Exit Sub
    End If
    Select Case MsgBox("Save the changes to " & DataFile & "?", vbYesNoCancel + vbQuestion, "Address book")
        Case vbYes
            SaveContacts
        Case vbCancel
            Cancel = 1
    End Select
End Sub

' The names that match Find, sorted (the list is Sorted).
Sub ShowNames()
    lstNames.Clear
    For Each key As String In contacts.Keys
        If Matches(key, txtFind.Text) Then
            lstNames.AddItem key
        End If
    Next
End Sub

Sub ShowContact(name As String)
    Dim c As Contact = contacts(name)
    txtName.Text = c.Name
    txtPhone.Text = c.Phone
    txtEmail.Text = c.Email
End Sub

Sub ClearDetails()
    txtName.Text = ""
    txtPhone.Text = ""
    txtEmail.Text = ""
End Sub

' Chooses name in the list, if Find shows it.
Sub Choose(name As String)
    For i As Integer = 0 To lstNames.ListCount - 1
        If lstNames.List(i) = name Then
            lstNames.ListIndex = i
            Exit Sub
        End If
    Next
End Sub

Sub txtFind_Change()
    ShowNames
End Sub

Sub lstNames_Click()
    If lstNames.ListIndex >= 0 Then
        ShowContact lstNames.Text
    End If
End Sub

' Keeps the details shown: a new contact, or a change to the one chosen
' (renamed, if its name was changed).
Sub cmdStore_Click()
    Dim name As String = Trim$(txtName.Text)
    If name = "" Then
        MsgBox "A contact needs a name.", vbExclamation, "Store"
        Exit Sub
    End If
    If lstNames.ListIndex >= 0 Then
        If lstNames.Text <> name Then
            contacts.Remove lstNames.Text
        End If
    End If
    Dim c As Contact
    c.Name = name
    c.Phone = Trim$(txtPhone.Text)
    c.Email = Trim$(txtEmail.Text)
    contacts(name) = c
    changed = True
    ShowNames
    Choose name
    lblStatus.Caption = "Stored " & name & " (" & contacts.Count & " contacts)."
End Sub

Sub cmdClear_Click()
    lstNames.ListIndex = -1
    ClearDetails
    lblStatus.Caption = "Type a new contact, then Store."
End Sub

Sub mnuNew_Click()
    cmdClear_Click
End Sub

Sub mnuDelete_Click()
    If lstNames.ListIndex < 0 Then
        MsgBox "Choose a contact to delete first.", vbExclamation, "Delete"
        Exit Sub
    End If
    Dim name As String = lstNames.Text
    If MsgBox("Delete " & name & "?", vbYesNo + vbQuestion, "Delete") = vbYes Then
        contacts.Remove name
        changed = True
        ShowNames
        ClearDetails
        lblStatus.Caption = "Deleted " & name & "."
    End If
End Sub

Sub mnuSave_Click()
    SaveContacts
    lblStatus.Caption = "Saved " & contacts.Count & " contacts to " & DataFile & "."
End Sub

Sub mnuExit_Click()
    Unload Me
End Sub
