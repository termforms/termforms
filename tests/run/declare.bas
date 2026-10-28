' Run test: Declare. Functions in a C file of the program (declare/utils.c,
' compiled with it) taking and returning numbers, Booleans and Strings; a
' ByRef Integer, a ByRef String filled as a buffer, a ByRef Type of
' numbers and a ByRef element (the array from it); CLong and CULong both
' ways; a String result that is NULL ("") ; an Alias; strlen from the C
' library with a CSizeT result; and a negative CULong, Overflow before the
' call. Platform names one of the three platforms (which one depends on
' where the test runs).
Begin Form frmMain
    Caption = "Declare"
    Width = 58
    Height = 12
    Begin Label lblLog
        Width = 56
        Height = 10
    End
End

Type Point
    X As Integer
    Y As Double
End Type

Declare Function add_tax Lib "declare/utils.c" (ByVal cents As Integer) As Integer
Declare Function halve Lib "declare/utils.c" (ByVal x As Double) As Double
Declare Function is_even Lib "declare/utils.c" (ByVal n As Long) As Boolean
Declare Sub bump Lib "declare/utils.c" (ByRef n As Integer)
Declare Sub fill_name Lib "declare/utils.c" (ByRef buffer As String)
Declare Function greeting Lib "declare/utils.c" (ByVal polite As Boolean) As String
Declare Function no_text Lib "declare/utils.c" () As String
Declare Function twice_long Lib "declare/utils.c" (ByVal n As CLong) As CLong
Declare Sub triple_long Lib "declare/utils.c" (ByRef n As CLong)
Declare Function bits Lib "declare/utils.c" (ByVal v As CULong) As CULong
Declare Sub move_point Lib "declare/utils.c" (ByRef p As Point)
Declare Function sum_bytes Lib "declare/utils.c" (ByRef first As Byte, ByVal count As Integer) As Integer
Declare Function Answer Lib "declare/utils.c" Alias "side_name" () As Integer
Declare Function strlen Lib "" (ByVal s As String) As CSizeT

Sub Note(s As String)
    lblLog.Caption = lblLog.Caption & s & " "
End Sub

Sub Form_Load()
    Note "tax " & add_tax(200) & ", half " & halve(5) & ", even " & is_even(6) & is_even(7)
    Dim n As Integer = 41
    bump n
    Note "bumped " & n
    Dim buffer As String = Space$(20)
    fill_name buffer
    Note "[" & buffer & "] " & Len(buffer)
    Note greeting(True) & "/" & greeting(False) & "/[" & no_text() & "]"
    Dim big As Long = 1000000
    triple_long big
    Note "long " & twice_long(21) & " " & big & " bits " & bits(255)
    Dim p As Point
    p.X = 1
    p.Y = 1.5
    move_point p
    Note "point " & p.X & "," & p.Y
    Dim bytes() As Byte = {1, 2, 3, 250}
    Note "sum " & sum_bytes(bytes(0), 4) & " from 1: " & sum_bytes(bytes(1), 3)
    Note "alias " & Answer() & ", strlen " & strlen("héllo")
    Try
        Note "bits " & bits(-1)
    Catch
        Note "E" & Err.Number
    End Try
    Note "platform known: " & (Platform = "Windows" Or Platform = "macOS" Or Platform = "Linux")
End Sub
