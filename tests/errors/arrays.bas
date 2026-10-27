' Errors test: array rules: constant sizes, dimensions, indexes, fixed vs
' dynamic, ByVal arrays read-only, whole arrays only where they belong.
Begin Form f
    Begin CommandButton b
    End
End

Sub Look(values() As Integer)
    values(0) = 1
    ReDim values(3)
    Erase values
End Sub

Sub Change(ByRef values() As Integer)
End Sub

Sub Main2()
    Dim n As Integer, d As Double, s As String
    Dim a(5) As Integer, g(2, 2) As String, dyn() As Integer, other() As Long
    Dim sized(n) As Integer
    Dim tooMany(1, 1, 1, 1, 1, 1, 1, 1, 1) As Integer
    a(1, 2) = 3
    g(1) = "x"
    a(d) = 1
    n = a
    a = dyn
    dyn = other
    dyn = n
    ReDim a(10)
    ReDim dyn(2, 2)
    ReDim dyn(d)
    ReDim n(3)
    s = a(0)
    Look g
    Change n
    n = UBound(a, 2)
    n = UBound(n)
    Erase n
End Sub

Sub Passes(values() As Integer)
    Change values
End Sub
