' Golden test: Types with nested records and fixed and dynamic array
' members; Type variables, arrays of Types, a Function returning a Type,
' ByVal (copied) and ByRef Type parameters, and a member of an element
' passed ByRef (its array locked meanwhile).
Begin Form frmMain
    Caption = "Types"
    Begin TextBox txtOut
        Width = 30
    End
End

Type Address
    City As String
    Zip As Integer
End Type

Type Customer
    Name As String
    Age As Integer
    Home As Address
    Scores(2) As Double
    Tags() As String
End Type

Dim best As Customer

Function Make(name As String, age As Integer) As Customer
    Make.Name = name
    Make.Age = age
    ReDim Make.Tags(0)
    Make.Tags(0) = "new"
End Function

Sub Birthday(ByRef c As Customer)
    c.Age = c.Age + 1
End Sub

Function Describe(c As Customer) As String
    c.Name = c.Name & "!"
    Describe = c.Name & " " & c.Age & " " & c.Home.City
End Function

Sub Bump(ByRef n As Integer)
    n = n + 1
End Sub

Sub Form_Load()
    Dim c As Customer, list(3) As Customer, s As String
    c = Make("Ann", 30)
    c.Home.City = "Oslo"
    c.Scores(1) = 2.5
    Birthday c
    list(0) = c
    list(0).Home.Zip = 1234
    Bump list(0).Home.Zip
    best = list(0)
    s = Describe(best) & " " & best.Name & " " & list(0).Tags(0) & UBound(list(0).Tags)
    txtOut.Text = s & Make("Bo", 1).Name
End Sub
