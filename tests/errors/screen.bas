' Errors test: Screen is read only, used through Width and Height, and
' its name is taken.
Sub Main()
    Dim Screen As Integer
    Dim n As Integer
    Screen.Width = 100
    n = Screen.Depth
    n = Screen
    n = Screen.Width + Screen.Height
End Sub
