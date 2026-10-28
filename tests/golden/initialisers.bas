' Golden test: Dim initialisers become assignments where the Dim is (in a
' loop, after the Dim's reset); a list becomes a ReDim and then each
' element; module variables are set by a hidden _init Sub that main calls
' once the forms exist, before the program starts.
Begin Form frmMain
    Caption = "Init"
End

Dim title As String = "T" & "1"
Dim sizes() As Integer = {1, 2}

Sub Form_Load()
    Dim i As Integer
    For i = 1 To 2
        Dim n As Integer = i * 2
        Me.Caption = CStr(n)
    Next
End Sub
