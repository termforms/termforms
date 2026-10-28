' Run test: Dim with values. Scalars (with As and with a suffix), several
' in one Dim, one using another; a Type, a Ref and an Enum; a local inside
' a loop given its value again on every pass; dynamic arrays from lists,
' one and two dimensions, and an empty list; module variables set at
' program start, in order, one calling a Function and one reading a
' control's form-block value, before Form_Load runs.
Begin Form frmMain
    Caption = "Init"
    Width = 58
    Height = 14
    Begin TextBox txtName
        Width = 10
        Text = "Ann"
    End
    Begin Label lblLog
        Top = 1
        Width = 56
        Height = 11
    End
End

Type Point
    X As Integer
    Y As Integer
End Type

Enum Size
    Small
    Large
End Enum

Dim started As String = "start"
Dim greeting As String = "Hi " & txtName.Text
Dim twice As Integer = Twice(21)
Dim primes() As Integer = {2, 3, 5, 7}

Function Twice(n As Integer) As Integer
    Twice = n * 2
End Function

Sub Note(s As String)
    lblLog.Caption = lblLog.Caption & s & " "
End Sub

Sub Form_Load()
    Note started & " " & greeting & " " & twice & " " & UBound(primes) & ":" & primes(3)
    Dim a As Integer = 5, b As Long = a * 2, c$ = "c"
    Note a & " " & b & " " & c
    Dim n%, m As Integer = 3
    Note n & " " & m
    Dim p As Point = MakePoint(1, 2)
    Dim r As Ref Point = New Point
    r.X = 9
    Dim z As Size = Size.Large
    Note p.X & "," & p.Y & " " & r.X & " " & z
    Dim i As Integer
    For i = 1 To 3
        Dim total As Integer = 10
        total = total + i
        Note "t" & total
    Next
    Dim words() As String = {"one", "two", "three"}
    Dim grid(,) As Integer = {{1, 2, 3}, {4, 5, 6}}
    Dim none() As Double = {}
    Dim count As Integer = 0
    Dim x As Double
    For Each x In none
        count = count + 1
    Next
    Note words(2) & " " & UBound(grid, 1) & "x" & UBound(grid, 2) & " " & grid(1, 2) & " empty:" & count
End Sub

Function MakePoint(x As Integer, y As Integer) As Point
    MakePoint.X = x
    MakePoint.Y = y
End Function
