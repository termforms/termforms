' Errors test: the string functions' rules: too few or too many
' arguments, a number where a String is wanted (and the reverse, and a
' Long where an Integer is), Split used as a single value, Join of an
' array that is not Strings, and a variable named after a function.
Begin Form f
    Begin CommandButton b
    End
End

Sub b_Click()
    Dim s As String, n As Integer, big As Long, nums(2) As Integer, Left As String
    s = Mid$(s)
    s = Replace(s, "a")
    n = InStr(1, s, "a", "b")
    s = UCase$(5)
    n = Len(n)
    s = Left$(s, big)
    s = Space(s)
    s = Split(s, ",")
    s = Join(nums, ",")
End Sub
