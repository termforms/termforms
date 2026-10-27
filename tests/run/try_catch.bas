' Run test: Try/Catch/Finally behaviour: Err's contents, When filters,
' Finally on every way out, errors crossing procedures, a failed
' assignment leaving the variable alone, rethrow, and errors raised inside
' Catch and Finally replacing the one being handled.
Begin Form f
    Caption = "Try"
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

Dim trail As String

Function Inner(n As Integer) As String
    Dim s As String
    s = "in" & n
    Inner = s & 100 \ n          ' fails for n = 0, after s is set
End Function

Function Middle(n As Integer) As String
    Middle = "[" & Inner(n) & "]"
End Function

Function Doubled(n As Integer) As Integer
    Try
        Return n * 2
    Finally
        trail = trail & "F"
    End Try
End Function

Sub Form_Load()
    Dim x As Integer, i As Integer, s As String

    ' Err describes the error; the failed assignment leaves x alone.
    x = 5
    Try
        x = 1 \ (x - 5)
    Catch
        t1.Text = Err.Number & " " & Err.Description & " line " & Err.Line & " x=" & x
    End Try

    ' When filters pick the Catch; errors cross two Functions.
    Try
        s = Middle(0)
    Catch When Err.Number = 6
        s = "overflow?"
    Catch When Err.Number = 11
        s = "div0 from line " & Err.Line
    End Try
    t2.Text = s & " " & Middle(4)

    ' Finally runs on normal completion, after an error, on Exit For, on Return.
    trail = ""
    For i = 1 To 5
        Try
            If i = 3 Then Exit For
            If i = 2 Then Throw 7000, "two"
            trail = trail & i
        Catch
            trail = trail & "c"
        Finally
            trail = trail & "f"
        End Try
    Next
    trail = trail & " i=" & i & " " & Doubled(21)
    t3.Text = trail

    ' Err.Raise with a source; Throw with a number gets VB's standard text.
    Try
        Err.Raise 5000, "Form_Load", "custom"
    Catch
        s = Err.Number & " " & Err.Source & " " & Err.Description
    End Try
    Try
        Throw 13
    Catch
        s = s & " / " & Err.Description
    End Try
    Err.Clear
    t4.Text = s & " / cleared " & Err.Number

    ' A bare Throw rethrows to the outer Try; an error in a Catch replaces
    ' the one being handled, and the Finally still runs.
    s = ""
    Try
        Try
            Throw 1001, "first"
        Catch
            Throw
        End Try
    Catch
        s = "outer got " & Err.Description
    End Try
    Try
        Try
            Throw 1002, "second"
        Catch
            x = CInt("oops")
        Finally
            s = s & ", finally ran"
        End Try
    Catch
        s = s & ", then " & Err.Number
    End Try
    t5.Text = s

    ' An error in a Finally replaces the one it was carrying.
    s = ""
    Try
        Try
            Throw 1003, "lost"
        Finally
            Throw 1004, "kept"
        End Try
    Catch
        s = Err.Number & " " & Err.Description
    End Try
    t6.Text = s
End Sub
