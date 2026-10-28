' Errors test: the Mid$ statement needs a String variable, element or
' field to change (not a literal, an Integer, a Function's result, or an
' element of an array passed ByVal), two or three arguments, and text.
Function Name() As String
    Name = "x"
End Function

Sub Change(ByVal words() As String)
    Mid$(words(0), 1) = "y"
End Sub

Sub Main()
    Dim s As String
    Dim n As Integer
    Mid$("abc", 1) = "x"
    Mid$(n, 1) = "x"
    Mid$(Name(), 1) = "x"
    Mid$(s) = "x"
    Mid$(s, 1) = 5
End Sub
