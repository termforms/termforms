' Errors test: Dim initialisers. One As for two names with a value; no
' type at all; a value for an As New, a Dictionary and a fixed array; a
' list for a non-array; the wrong number of dimensions; rows of different
' lengths. (Values of the wrong type: initialisers_types.bas.)
Type Point
    X As Integer
End Type

Sub Main()
    Dim a, b As Integer = 5
    Dim c = 5
    Dim p As New Point = Nothing
    Dim d As Dictionary(Of Integer) = 1
    Dim f(2) As Integer = {1, 2, 3}
    Dim n As Integer = {1}
    Dim g() As Integer = {{1}, {2}}
    Dim h(,) As Integer = {{1, 2}, {3}}
End Sub
