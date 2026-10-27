' Golden test: With on a Type variable (nothing kept), a nested With, an
' array element (its index kept), a Ref (kept, retained) and a Function's
' Type result (a copy kept); a .Member passed ByRef.
Begin Form frmMain
    Caption = "With"
    Begin TextBox txtOut
        Width = 30
    End
End

Type Address
    City As String
End Type

Type Customer
    Name As String
    Age As Integer
    Home As Address
End Type

Function Make() As Customer
    Make.Name = "new"
End Function

Sub Bump(ByRef n As Integer)
    n = n + 1
End Sub

Sub Form_Load()
    Dim c As Customer, list(3) As Customer, r As New Customer, i As Integer
    With c
        .Name = "Ann"
        With .Home
            .City = "Oslo"
        End With
        Bump .Age
    End With
    With list(i + 1)
        .Age = 2
    End With
    With r
        .Home.City = c.Home.City
    End With
    With Make()
        txtOut.Text = .Name & r.Home.City
    End With
End Sub
