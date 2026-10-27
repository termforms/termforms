' Golden test: Refs (the run test's program, for the shape of the C).
' values, Is/IsNot, a linked list built and walked, Ref members and arrays
' of Refs, ByVal Refs (the caller's variable is not rebound) and ByRef,
' Dim As New, converting values and Refs, error 91 caught, and a Function
' returning a Type with Return.
Begin Form f
    Caption = "Refs"
    Width = 56
    Height = 10
    Begin TextBox t1
        Width = 50
    End
    Begin TextBox t2
        Top = 1
        Width = 50
    End
    Begin TextBox t3
        Top = 2
        Width = 50
    End
    Begin TextBox t4
        Top = 3
        Width = 50
    End
    Begin TextBox t5
        Top = 4
        Width = 50
    End
End

Type Node
    Value As Integer
    Next As Ref Node
End Type

Type Point
    X As Integer
End Type

Function Push(head As Ref Node, value As Integer) As Ref Node
    Dim n As New Node
    n.Value = value
    n.Next = head
    Return n
End Function

Function Total(head As Ref Node) As Integer
    Dim p As Ref Node
    p = head
    While p IsNot Nothing
        Total = Total + p.Value
        p = p.Next
    Wend
End Function

Sub Rebind(r As Ref Node)                 ' ByVal: only the copy is rebound
    r.Value = 100                         ' ...but the shared record changes
    r = Nothing
End Sub

Sub Clear(ByRef r As Ref Node)
    r = Nothing
End Sub

Function MakePoint(x As Integer) As Point
    Dim p As Point
    p.X = x
    Return p
End Function

Sub Form_Load()
    Dim a As Ref Node, b As Ref Node, list As Ref Node, i As Integer, s As String
    Dim v As Point, rp As Ref Point, all(2) As Ref Point

    a = New Node
    a.Value = 1
    b = a
    b.Value = 2
    t1.Text = a.Value & " " & (a Is b) & " " & (a IsNot Nothing) & " " & (New Node Is a)

    For i = 1 To 4
        list = Push(list, i)
    Next
    s = "sum=" & Total(list) & " " & list.Value & list.Next.Value
    Rebind list
    s = s & " after=" & list.Value & " " & (list IsNot Nothing)
    Clear list
    t2.Text = s & " cleared=" & (list Is Nothing)

    v = MakePoint(7)
    rp = v                                ' a new record, copied from v
    rp.X = 8
    v = rp                                ' copied back
    v.X = v.X + 1
    all(0) = rp
    all(1) = New Point
    all(1).X = 5
    t3.Text = v.X & " " & rp.X & " " & all(0).X & all(1).X & " " & (all(2) Is Nothing)

    Try
        i = all(2).X
    Catch
        t4.Text = Err.Number & " " & Err.Description
    End Try

    Dim fresh As New Point
    fresh.X = 3
    t5.Text = "fresh=" & fresh.X & " " & MakePoint(9).X
End Sub
