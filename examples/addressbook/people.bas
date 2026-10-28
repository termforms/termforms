' people.bas -- the address book's data: a Contact for each name, kept
' in a Dictionary by name, read from and written to a tab-separated file.

Type Contact
    Name As String
    Phone As String
    Email As String
End Type

Public Const DataFile = "addresses.txt"

Public contacts As Dictionary(Of Contact)
Public changed As Boolean ' since the file was last read or written

' Reads the file, if there is one; a line without three fields is passed
' over (it was not written by this program).
Sub LoadContacts()
    If Not Exists(DataFile) Then
        Exit Sub
    End If
    Dim n As Integer = FreeFile
    Open DataFile For Input As #n
    Do While Not EOF(n)
        Dim line As String
        Line Input #n, line
        Dim fields() As String = Split(line, vbTab)
        If UBound(fields) = 2 Then
            Dim c As Contact
            c.Name = fields(0)
            c.Phone = fields(1)
            c.Email = fields(2)
            contacts(c.Name) = c
        End If
    Loop
    Close #n
    changed = False
End Sub

Sub SaveContacts()
    Dim n As Integer = FreeFile
    Open DataFile For Output As #n
    For Each key As String In contacts.Keys
        Dim c As Contact = contacts(key)
        Print #n, c.Name & vbTab & c.Phone & vbTab & c.Email
    Next
    Close #n
    changed = False
End Sub

' Whether a name contains the text searched for, case ignored ("" finds
' every name).
Function Matches(name As String, find As String) As Boolean
    Return find = "" Or InStr(LCase$(name), LCase$(find)) > 0
End Function
