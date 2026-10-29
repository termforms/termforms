' Files() and Folders() give arrays: not a single String, and not into a
' fixed array; IsLink takes a String.
Sub Main()
    Dim s As String
    Dim fixed(3) As String
    Dim names() As String
    s = Files("*")
    names = Folders(3)
    fixed = Files("*")
    If IsLink(1) Then s = ""
End Sub
