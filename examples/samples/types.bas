' types.bas -- Types and Refs: a Type is a value (assigning copies it);
' a Ref shares one record (New makes it, Nothing is none); Refs build a
' linked list, as a Type cannot hold itself.
'
' Build and run:   tfc types.bas && ./types
Begin Form frmSample
    Caption = "Types and Refs"
    Width = 70
    Height = 20
    Begin ListBox lstOut
        Left = 1
        Width = 66
        Height = 17
    End
End

' Each line of the sample's results.
Sub Note(text As String)
    lstOut.AddItem text
End Sub

Type Point
    X As Integer
    Y As Integer
End Type

Type Node
    Value As String
    NextNode As Ref Node
End Type

Sub Form_Load()
    Dim a As Point
    a.X = 1
    a.Y = 2
    Dim b As Point = a ' a copy
    b.X = 99
    Note "values: a.X = " & a.X & ", b.X = " & b.X & " (b is a copy)"

    Dim r As Ref Point = New Point
    r.X = 5
    Dim s As Ref Point = r ' the same record
    s.X = 7
    Note "Refs: r.X = " & r.X & " (s and r share it); r Is s: " & (r Is s)

    Dim none As Ref Point
    Note "a new Ref is Nothing: " & (none Is Nothing)
    Try
        Note "none.X is " & none.X
    Catch
        Note "using Nothing is error " & Err.Number & ": " & Err.Description
    End Try

    ' A list built at its head: c, then b, then a.
    Dim head As Ref Node
    For Each word As String In Split("a b c")
        Dim n As Ref Node = New Node
        n.Value = word
        n.NextNode = head
        head = n
    Next
    Dim walk As Ref Node = head
    Dim text As String
    Do While walk IsNot Nothing
        text = text & walk.Value & " "
        walk = walk.NextNode
    Loop
    Note "linked list: " & text
End Sub
