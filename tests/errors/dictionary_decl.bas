' Errors test: Dictionary declarations the parser rejects: New (it is
' ready at its Dim), an array of Dictionaries, a Dictionary of
' Dictionaries, and a Const.
Sub Main()
    Dim e As New Dictionary(Of Integer)
    Dim many(2) As Dictionary(Of Integer)
    Dim nested As Dictionary(Of Dictionary(Of Integer))
    Const C As Dictionary(Of Integer) = 1
End Sub
