' The code module of the inventory project (run test).
Public total As Integer
Dim adds As Integer

Sub Add(n As Integer)
    total = total + n
    adds = adds + 1
End Sub

Function Count() As Integer
    Return adds
End Function
