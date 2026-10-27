' Run test: Dictionary(Of T): d(key) adding and replacing, Add, Exists,
' Remove (the order of the rest kept), Count, Clear, case-sensitive keys,
' Keys and Items (into arrays and in For Each), copying on assignment,
' ByVal (read) and ByRef (changed) parameters, Types and Refs as values,
' a Dictionary in a Type, an entry passed ByRef and in a With, and errors
' 5 (no such key), 457 (Add of a key that is there) and 10 (adding while
' an entry is passed ByRef), caught.
Begin Form f
    Caption = "Dictionary"
    Width = 56
    Height = 10
    Begin TextBox t1
        Width = 50
    End
    Begin TextBox t2
        Top = 1
        Width = 50
    End
    Begin TextBox t3
        Top = 2
        Width = 50
    End
    Begin TextBox t4
        Top = 3
        Width = 50
    End
    Begin TextBox t5
        Top = 4
        Width = 50
    End
End

Type Person
    Name As String
    Age As Integer
End Type

Type Club
    Members As Dictionary(Of Person)
End Type

Dim ages As Dictionary(Of Integer)

Function Total(d As Dictionary(Of Integer)) As Integer
    Dim k As String
    For Each k In d.Keys
        Total = Total + d(k)
    Next
End Function

Sub AddOne(ByRef d As Dictionary(Of Integer), key As String)
    d(key) = d(key) + 1
End Sub

Sub Bump(ByRef n As Integer)
    n = n + 1
    ages("during") = 0            ' adding while an entry is passed ByRef: 10
End Sub

Sub Form_Load()
    Dim s As String, k As String, n As Integer, keys() As String, copy As Dictionary(Of Integer)
    ages("Ann") = 30
    ages("Bo") = 25
    ages.Add "Cy", 41
    ages("Ann") = ages("Ann") + 1 ' replaces
    ages("ann") = 1               ' another key: case matters
    AddOne ages, "Bo"
    t1.Text = ages.Count & " " & ages("Ann") & " " & ages("Bo") & " " & ages.Exists("ANN") & " " & Total(ages)

    copy = ages
    copy("Cy") = 0
    ages.Remove "Bo"
    keys = ages.Keys
    For Each k In keys
        s = s & k & ","
    Next
    For Each n In ages.Items
        s = s & n & ";"
    Next
    t2.Text = s & " " & copy("Cy") & " " & copy.Count & " " & UBound(keys)

    Dim club As Club, p As Person, r As Dictionary(Of Ref Person), q As New Person
    p.Name = "Di"
    p.Age = 9
    club.Members("di") = p
    p.Age = 10                    ' the entry is a copy
    club.Members("di").Age = club.Members("di").Age + 1
    With club.Members("di")
        .Name = .Name & "!"
    End With
    q.Name = "Ed"
    r.Add "ed", q
    q.Age = 5                     ' a Ref: shared
    t3.Text = club.Members("di").Name & club.Members("di").Age & " " & p.Age & " " & r("ed").Age & club.Members.Count

    s = ""
    Try
        n = ages("nobody")
    Catch When Err.Number = 5
        s = s & Err.Number
    End Try
    Try
        ages.Add "Cy", 1
    Catch
        s = s & " " & Err.Number
    End Try
    Try
        Bump ages("Cy")
    Catch
        s = s & " " & Err.Number & " " & ages("Cy") & " " & ages.Exists("during")
    End Try
    Try
        ages.Remove "Bo"
    Catch
        s = s & " " & Err.Number
    End Try
    t4.Text = s

    ages.Clear
    ages.Add "", 7                ' "" is a key like any other
    t5.Text = ages.Count & " " & ages("") & " " & copy.Count
End Sub
