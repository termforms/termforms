' Golden test: Consts (typed by value, As and suffix; built from other
' Consts), Enums (Integer and Long, explicit and counted values), Enum
' variables, parameters and results, CType, and For over an Enum.
Begin Form frmMain
    Caption = "Constants"
    Begin TextBox txtOut
        Width = 30
    End
End

Const MAX_ITEMS = 100
Const LIMIT = MAX_ITEMS * 2 + 1
Const RATE = 0.075
Const TITLE = "Orders" & " " & MAX_ITEMS
Const BIG As Long = 3
Const FLAG = MAX_ITEMS > 50 And Not False
Const HALF# = 1 / 2

Enum Color
    Red
    Green
    Blue = 10
    Violet
End Enum

Enum Size As Long
    Small = 5000000000
    Large
End Enum

Dim current As Color

Function Next1(c As Color) As Color
    Next1 = CType(c + 1, Color)
End Function

Sub Form_Load()
    Const LOCAL_TEXT = "x"
    Dim i As Integer, s As String, n As Long
    current = Color.Blue
    current = Next1(current)
    i = current + LIMIT
    n = Size.Large + BIG
    For current = Color.Red To Color.Violet Step 5
        s = s & current & LOCAL_TEXT
    Next
    If FLAG And current <> Color.Green Then s = s & TITLE & RATE & HALF & CType("2", Color)
    txtOut.Text = s & i & n
End Sub
