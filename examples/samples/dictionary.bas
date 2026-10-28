' dictionary.bas -- a Dictionary maps String keys to values (here, how
' often each word comes): d(key) reads or sets one, Exists, Remove, Count,
' and For Each over its Keys, in the order they were added.
'
' Build and run:   tfc dictionary.bas && ./dictionary
Begin Form frmSample
    Caption = "Dictionary"
    Width = 70
    Height = 20
    Begin ListBox lstOut
        Left = 1
        Width = 66
        Height = 17
    End
End

' Each line of the sample's results.
Sub Note(text As String)
    lstOut.AddItem text
End Sub

Type Capital
    City As String
    People As Long
End Type

Sub Form_Load()
    Dim counts As Dictionary(Of Integer)
    For Each word As String In Split("the cat sat on the mat the end")
        If counts.Exists(word) Then
            counts(word) = counts(word) + 1
        Else
            counts(word) = 1
        End If
    Next
    For Each word As String In counts.Keys
        Note word & ": " & counts(word)
    Next
    Note counts.Count & " different words; ""the"" " & counts("the") & " times"
    counts.Remove "the"
    Note "after Remove ""the"": " & counts.Count & " words, Exists(""the"") " & counts.Exists("the")

    Dim capitals As Dictionary(Of Capital)
    Dim c As Capital
    c.City = "Canberra"
    c.People = 470000
    capitals("Australia") = c
    c.City = "Wellington"
    c.People = 215000
    capitals("New Zealand") = c
    For Each country As String In capitals.Keys
        Note country & ": " & capitals(country).City & ", " & capitals(country).People & " people"
    Next
    Try
        Note capitals("France").City
    Catch
        Note "a key not there is error " & Err.Number & " (" & Err.Description & ")"
    End Try
End Sub
