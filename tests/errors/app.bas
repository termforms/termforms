' Errors: App is a built-in object with read-only properties, and
' CommandArgs gives an array.
Sub Main()
    Dim App As Integer
    Dim s As String = CommandArgs()
    s = App.Name
    s = App
    App.Path = "x"
    s = CommandArgs(1)
End Sub
