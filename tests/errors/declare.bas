' Errors test: Declare's rules. A whole array, a ByVal Type, a Type with a
' String in it, a Ref; a Type result; a C name C keeps for itself and one
' tfc's C uses; one C function declared twice with different types; CLong
' outside a Declare. (The Libs' own checks: declare_files.bas.)
Type Plain
    X As Integer
End Type

Type Named
    Name As String
End Type

Declare Sub takes_array Lib "" (ByRef a() As Byte)
Declare Sub takes_type Lib "" (ByVal p As Plain)
Declare Sub takes_named Lib "" (ByRef n As Named)
Declare Sub takes_ref Lib "" (ByVal r As Ref Plain)
Declare Function gives_type Lib "" () As Plain
Declare Function Double2 Lib "" Alias "double" () As Double
Declare Function tf_thing Lib "" () As Integer
Declare Function twin Lib "" (ByVal n As Integer) As Integer
Declare Function Twin2 Lib "" Alias "twin" (ByVal n As Long) As Integer

Sub Main()
    Dim n As CLong
End Sub
