' Errors test: Const and Enum rules: constant expressions, Option Strict
' with Enum types, qualification, reserved uses, and compile-time overflow.
Begin Form f
    Begin CommandButton b
    End
End

Dim v As Integer

Const FROM_VAR = v + 1
Const TOO_BIG = 2147483647 + 1
Const DIV = 1 \ 0
Const NARROW As Integer = 3000000000
Const TEXT As String = 5
Const FROM_CALL = CInt(2.5)

Enum Color
    Red
    Red
    Blue = 1.5
End Enum

Enum Pet
    Cat
    Dog
End Enum

Sub Takes(c As Color)
End Sub

Sub Main2()
    Dim c As Color, p As Pet, i As Integer
    c = 1
    c = p
    c = Pet.Cat
    i = c
    Takes 0
    c = Red
    c = Color.Purple
    c = Color
    Color.Red = c
    v = FROM_VAR
    If c = p Then i = 1
    Dim x As Colour
    i = CType(c, Boolean)
End Sub

Sub Main3()
    Const LIMIT = 10
    LIMIT = 11
End Sub
