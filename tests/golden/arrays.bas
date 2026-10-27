' Golden test: fixed and dynamic arrays of one, two and three dimensions,
' ReDim Preserve, Erase, whole-array copy, UBound, array parameters ByVal
' and ByRef, elements passed ByRef (locking the array), a Dim inside a loop.
Begin Form frmMain
    Caption = "Arrays"
    Begin TextBox txtOut
        Width = 30
    End
End

Const SIZE = 4

Dim totals(SIZE) As Long
Dim names() As String

Function Sum(values() As Integer) As Integer
    Dim i As Integer
    For i = 0 To UBound(values)
        Sum = Sum + values(i)
    Next
End Function

Sub Grow(ByRef values() As Integer, extra As Integer)
    ReDim Preserve values(UBound(values) + extra)
End Sub

Sub Bump(ByRef n As Integer)
    n = n + 1
End Sub

Sub Form_Load()
    Dim a(3) As Integer, grid(2, 3) As String, cube(1, 1, 1) As Double
    Dim d() As Integer, copy() As Integer, i As Integer
    a(0) = 5
    a(3) = a(0) * 2
    grid(2, 3) = "corner"
    cube(1, 0, 1) = 1.5
    ReDim d(2)
    d(1) = Sum(a)
    Grow d, 3
    Bump d(4)
    copy = d
    ReDim names(1)
    names(1) = grid(2, 3) & UBound(grid, 2)
    totals(SIZE) = CLng(d(4) + cube(1, 0, 1))
    For i = 0 To 1
        Dim row(2) As Integer
        row(i) = i
    Next
    Erase d
    txtOut.Text = names(1) & copy(1) & totals(SIZE)
End Sub
