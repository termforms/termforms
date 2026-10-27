' Run test: With ... End With on a Type variable, a nested With, an array
' element (its index evaluated once), a Ref (kept even when the variable
' is changed inside), Functions returning a Ref and a Type, a control;
' .Member passed ByRef, ReDim and UBound of an array member, With in a
' loop left by Exit For, ReDim taking the element away (error 9) and a
' With on Nothing (error 91), both caught.
Begin Form f
    Caption = "With"
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

Type Address
    City As String
    Zip As Integer
End Type

Type Person
    Name As String
    Age As Integer
    Home As Address
    Tags() As String
End Type

Dim shared As Ref Person

Function Shared() As Ref Person
    Return shared
End Function

Function Make(name As String) As Person
    Make.Name = name & "!"
    Make.Age = 7
End Function

Sub Bump(ByRef n As Integer)
    n = n + 1
End Sub

Sub Form_Load()
    Dim p As Person, people() As Person, i As Integer, r As Ref Person, s As String
    With p
        .Name = "Ann"
        .Age = 30
        With .Home
            .City = "Oslo"
            .Zip = 150
        End With
        Bump .Age
        ReDim .Tags(1)
        .Tags(1) = "x"
        s = .Name & " " & .Age & " " & .Home.City & " " & UBound(.Tags)
    End With
    t1.Text = s & " " & p.Home.Zip & p.Tags(1)

    ReDim people(2)
    i = 1
    With people(i)
        i = 2                     ' the element was chosen at the With
        .Name = "Bo"
    End With
    s = people(1).Name & "/" & people(2).Name & "/"
    For i = 0 To 2
        With people(i)
            .Age = i * 10
            If i = 1 Then Exit For
        End With
    Next
    t2.Text = s & people(0).Age & " " & people(1).Age & " " & people(2).Age & " " & i

    shared = New Person
    r = shared
    With r
        r = Nothing               ' the With still has the record
        .Name = "Cy"
    End With
    With Shared()
        .Age = 41
    End With
    With Make("Di")
        s = .Name & .Age
    End With
    t3.Text = shared.Name & " " & shared.Age & " " & s & " " & (r Is Nothing)

    Try
        With people(2)
            ReDim Preserve people(2)
            .Name = "kept"
            s = people(2).Name
            ReDim people(0)
            .Name = "gone"
        End With
    Catch
        s = s & "|" & Err.Number
    End Try
    Try
        With r
            .Age = 1
        End With
    Catch When Err.Number = 91
        s = s & " " & Err.Number
    End Try
    t4.Text = s

    With t5
        .Text = "control" & .Text
    End With
End Sub
