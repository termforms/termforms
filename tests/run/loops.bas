' Run test: For (steps, empty ranges, limits evaluated once), Do loops
' testing at the top and bottom, While/Wend, Exit from nested loops, and
' a Dim inside a loop starting afresh on every pass.
Begin Form f
    Caption = "Loops"
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

Function MinusOne() As Integer
    Return -1
End Function

Sub Form_Load()
    Dim i As Integer, j As Integer, n As Integer, s As String

    For i = 1 To 10
        n = n + i
    Next
    s = n & ":"
    For i = 10 To 1 Step -3
        s = s & " " & i
    Next i
    n = 0
    For i = 5 To 1
        n = n + 1
    Next
    t1.Text = s & " empty=" & n & " after=" & i

    n = 0
    Do While n < 5
        n = n + 1
    Loop
    s = "" & n
    Do Until n >= 8
        n = n + 1
    Loop
    s = s & " " & n
    n = 10
    Do
        n = n + 1
    Loop While n < 3
    s = s & " " & n
    Do
        n = n - 4
    Loop Until n < 0
    t2.Text = s & " " & n

    n = 0
    While n < 3
        n = n + 1
    Wend
    While n < 6
        n = n + 2
    End While
    s = "" & n
    Do
        For i = 1 To 6
            For j = 1 To 6
                If i * j = 12 Then
                    s = s & " " & i & "x" & j
                    Exit Do
                End If
            Next
        Next
        s = s & " not reached"
    Loop
    t3.Text = s

    s = ""
    For i = 1 To 3
        Dim piece As String
        piece = piece & i
        s = s & piece
    Next
    t4.Text = s

    s = ""
    For i = 3 To MinusOne() Step MinusOne()
        If i = 0 Then Exit For
        s = s & i
    Next
    t5.Text = s & " i=" & i

    n = 3
    j = 0
    For i = 1 To n
        n = 10
        j = j + 1
    Next
    t6.Text = j & " passes, n=" & n
End Sub
