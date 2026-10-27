' Errors test: Dictionary rules: a key that is not a String, the
' Dictionary used as a value or an array (ReDim, Erase, UBound, For Each),
' members it does not have, Add with too few arguments, Exists without a
' key, changing a ByVal Dictionary, Keys into a fixed array or a single
' String, mixing Dictionaries and arrays, and a Function returning one.
' (Declarations the parser rejects are in dictionary_decl.bas.)
Begin Form f
    Begin CommandButton b
    End
End

Function Make() As Dictionary(Of Integer)
End Function

Sub Look(d As Dictionary(Of Integer))
    d("x") = 1
    d.Clear
End Sub

Sub b_Click()
    Dim d As Dictionary(Of Integer), n As Integer, s As String, a() As Integer
    Dim fixed(2) As String, names As Dictionary(Of String)
    n = d(1)
    n = d
    ReDim d(3)
    Erase d
    n = UBound(d)
    For Each n In d
    Next
    n = d.Size
    d.Add "x"
    If d.Exists Then n = 1
    fixed = names.Keys
    s = names.Keys
    a = d
    d = names
    Look a
End Sub
