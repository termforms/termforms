' Golden test: a Dictionary of Integers (put, get, Add, Exists, Remove,
' Count, Clear), of a Type in a Type, Keys into an array and in For Each,
' and an entry passed ByRef (the Dictionary locked meanwhile).
Begin Form frmMain
    Caption = "Dictionary"
    Begin TextBox txtOut
        Width = 30
    End
End

Type Person
    Name As String
End Type

Type Club
    Members As Dictionary(Of Person)
End Type

Sub Bump(ByRef n As Integer)
    n = n + 1
End Sub

Sub Form_Load()
    Dim ages As Dictionary(Of Integer), club As Club, p As Person, keys() As String, k As String
    ages("Ann") = 30
    ages.Add "Bo", ages("Ann") + 1
    Bump ages("Bo")
    If ages.Exists("Ann") Then ages.Remove "Ann"
    club.Members("x") = p
    club.Members("x").Name = "Cy"
    keys = ages.Keys
    For Each k In club.Members.Keys
        txtOut.Text = k & ages.Count
    Next
    ages.Clear
End Sub
