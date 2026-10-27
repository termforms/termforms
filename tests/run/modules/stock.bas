' Module of the modules run test: a Public Type, Const, variable and
' Subs/Functions, and a Private variable with the same name as one in
' the main module.

Public Type Item
    Name As String
    Qty As Integer
End Type

Public Const MAX_ITEMS = 10
Public total As Integer
Dim hidden As Integer
Dim items(9) As Item
Dim used As Integer

Sub AddStock(name As String, qty As Integer)
    If qty < 0 Then Throw 1001, "a quantity cannot be negative"
    items(used).Name = name
    items(used).Qty = qty
    used = used + 1
    total = total + qty
    hidden = hidden + 10
End Sub

Function Lookup(name As String) As Item
    Dim i As Integer
    For i = 0 To used - 1
        If items(i).Name = name Then Return items(i)
    Next
End Function

Function Hidden() As Integer
    Return hidden
End Function

Function Label() As String
    Return "stock"
End Function

Private Function Twice(n As Integer) As Integer
    Return n * 3                ' never called: report's Public Twice is
End Function
