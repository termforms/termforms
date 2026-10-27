' Run test: each error the runtime raises has VB6's number and wording,
' whether the runtime raises it or a program Throws the number.
Begin Form f
    Caption = "Error numbers"
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
    Begin TextBox t6
        Top = 5
        Width = 50
    End
End

' The runtime's number and text for the error, then "(same)" if Throw with
' the same number gives the same text.
Function Both(n As Integer) As String
    Dim zero As Integer, d As Double, i As Integer, raised As String
    Try
        If n = 5 Then d = (-8) ^ 0.5
        If n = 6 Then i = 2147483647 + zero + 1
        If n = 11 Then i = 1 \ zero
        If n = 13 Then i = CInt("x")
        If n = 7 Then Throw 7       ' out of memory is never raised catchably
    Catch
        raised = Err.Description
        Both = Err.Number & " " & raised
    End Try
    Try
        Throw n
    Catch
        If Err.Description = raised Then
            Both = Both & " (same)"
        Else
            Both = Both & n & " " & Err.Description
        End If
    End Try
End Function

Sub Form_Load()
    t1.Text = Both(5)
    t2.Text = Both(6)
    t3.Text = Both(7)
    t4.Text = Both(11)
    t5.Text = Both(13)
    t6.Text = Both(1000)
End Sub
