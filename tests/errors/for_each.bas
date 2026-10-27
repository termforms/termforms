' Errors test: For Each rules: something that is not an array, elements
' the variable cannot hold (Option Strict), a Const or an array as the loop
' variable, and a Next naming another variable.
Begin Form f
    Begin CommandButton b
    End
End

Const LIMIT = 3

Sub b_Click()
    Dim n As Integer, total As Long, names(2) As String, longs(1) As Long, grid(1) As Integer
    For Each n In total
    Next
    For Each n In names
    Next
    For Each n In longs
    Next
    For Each LIMIT In grid
    Next
    For Each grid In grid
    Next
    For Each n In grid
    Next total
End Sub
