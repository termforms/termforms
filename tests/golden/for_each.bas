' Golden test: For Each over a local array (nothing kept), an array member
' of an element (its index kept), and a Ref's array member (the Ref kept);
' Strings and Types as loop variables; Exit For.
Begin Form frmMain
    Caption = "For Each"
    Begin TextBox txtOut
        Width = 30
    End
End

Type Order
    Lines() As String
End Type

Sub Form_Load()
    Dim words(2) As String, orders(1) As Order, o As Order, r As New Order, w As String, i As Integer
    For Each w In words
        If w = "x" Then Exit For
    Next
    For Each w In orders(i + 1).Lines
        txtOut.Text = w
    Next
    For Each o In orders
        i = i + 1
    Next
    For Each w In r.Lines
        txtOut.Text = txtOut.Text & w
    Next w
End Sub
