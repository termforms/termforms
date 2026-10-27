' Golden test: For with literal, variable and negative steps; Do loops
' testing at the top and bottom; While/Wend and While/End While; Exit
' from inner and outer loops; block-scoped Dims inside loops.
Begin Form frmMain
    Caption = "Loops"
    Begin TextBox txtOut
        Width = 30
    End
End

Function Limit() As Integer
    Limit = 3
End Function

Sub Form_Load()
    Dim i As Integer, j As Integer, total As Integer, s As String
    For i = 1 To 10
        total = total + i
    Next i
    For i = 10 To 1 Step -2
        total = total - i
    Next
    For i = total To Limit() Step j
        Dim n As Integer
        n = n + i
    Next
    Do While total < 100
        total = total * 2
    Loop
    Do
        total = total - 1
    Loop Until total & "" = "50"
    While total > 40
        total = total - 3
    Wend
    While total > 30
        total = total - 1
    End While
    Do
        For i = 1 To 5
            For j = 1 To 5
                If i * j = 6 Then Exit For
                If i * j = 12 Then
                    Dim s2 As String
                    s2 = s2 & i
                    s = s2
                    Exit Do
                End If
            Next
            Do
                Dim s2 As String
                s2 = "inner"
                If i = 4 Then Exit For
                Exit Do
            Loop
        Next
    Loop
    txtOut.Text = "" & total & s
End Sub
