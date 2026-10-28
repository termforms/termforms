' arrays.bas -- arrays start at 0 (Dim a(3) has four); dynamic arrays grow
' with ReDim Preserve; two dimensions; For Each; assigning copies;
' Split and Join.
'
' Build and run:   tfc arrays.bas && ./arrays
Begin Form frmSample
    Caption = "Arrays"
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

Sub Form_Load()
    Dim fixed(3) As Integer
    For i As Integer = 0 To UBound(fixed)
        fixed(i) = i * i
    Next
    Note "Dim fixed(3): " & (UBound(fixed) + 1) & " elements, the last " & fixed(3)

    Dim grow() As String
    ReDim grow(0)
    grow(0) = "one"
    ReDim Preserve grow(2)
    grow(1) = "two"
    grow(2) = "three"
    Note "after ReDim Preserve: " & Join(grow, ", ")

    Dim grid(,) As Integer = {{1, 2, 3}, {4, 5, 6}}
    Note "grid(1, 2) = " & grid(1, 2) & "; " & (UBound(grid, 1) + 1) & " rows of " & (UBound(grid, 2) + 1)

    Dim total As Integer = 0
    For Each n As Integer In grid
        total = total + n
    Next
    Note "For Each over the grid: total " & total

    Dim copy() As String = grow ' a copy, not the same array
    copy(0) = "ONE"
    Note "after copy(0) = ""ONE"": grow(0) is still " & grow(0)

    Dim parts() As String = Split("2026-10-28", "-")
    Note "Split a date: year " & parts(0) & ", month " & parts(1) & ", day " & parts(2)
End Sub
