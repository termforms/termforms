' Run test: binary files: Open For Binary (made if missing), Put at the
' current position and at an offset past the end (the gap filled with
' zeros), Get by the buffer's size and by a count (resizing it), reads
' past the end (a dynamic buffer shortened, a fixed one's rest zeroed),
' Seek # and Seek(), LOF, EOF, and errors 54, 63 and 9, caught.
Begin Form f
    Caption = "Binary"
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
End

Function Show(b() As Byte) As String
    Dim x As Byte
    For Each x In b
        Show = Show & x & ","
    Next
    Show = Show & "(" & UBound(b) & ")"
End Function

Sub Form_Load()
    Dim buf(9) As Byte, pair(1) As Byte, fixed(3) As Byte, rd() As Byte, i As Integer, s As String
    For i = 0 To 9
        buf(i) = CByte(i * 10)
    Next
    pair(0) = 255
    pair(1) = 7
    Open "data.bin" For Binary As #1
    Put #1, , buf                 ' bytes 0-9
    Put #1, 20, pair              ' bytes 20-21; 10-19 become 0
    t1.Text = LOF(1) & " " & Seek(1)

    Seek #1, 0
    ReDim rd(3)
    Get #1, , rd                  ' 4 bytes: the buffer's size
    s = Show(rd) & " at " & Seek(1)
    Get #1, 5, rd, 2              ' a count: rd becomes 2 long
    t2.Text = s & " " & Show(rd)

    Get #1, 20, rd, 10            ' only 2 bytes there
    s = Show(rd) & " " & EOF(1)
    Get #1, 30, rd, 4             ' none
    s = s & " " & Show(rd)
    Get #1, 20, fixed             ' a fixed buffer: the rest is 0
    t3.Text = s & " " & Show(fixed)
    Close #1

    s = ""
    Open "text.txt" For Output As #2
    Try
        Get #2, 0, rd
    Catch When Err.Number = 54
        s = "54"
    End Try
    Close #2
    Open "data.bin" For Binary As #1
    Try
        Get #1, -1, rd
    Catch
        s = s & " " & Err.Number
    End Try
    Try
        Put #1, 0, pair, 3
    Catch
        s = s & " " & Err.Number
    End Try
    Close #1
    t4.Text = s
End Sub
