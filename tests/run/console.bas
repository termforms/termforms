' Run test: a console program (Sub Main, no forms). Line Input with and
' without a prompt from standard input (console.stdin: a CRLF line, one
' beyond ASCII, a last line with no line end), EOF(0), error 62 reading
' past the end, Print's forms (items joined, a lone number, a trailing ;,
' an empty Print), Print #0 to standard error (console.stderr), and End n
' for the exit code (console.exit).
Sub Main()
    Dim name As String
    Line Input "Your name: "; name
    Print "Hello, "; name; "!"
    Print 42
    Print
    Print "one";
    Print " line"
    Dim count As Integer
    Do While Not EOF(0)
        Dim line As String
        Line Input line
        count = count + 1
        Print count; ": ["; line; "] "; Len(line)
    Loop
    Try
        Line Input name
    Catch
        Print "past the end: "; Err.Number
    End Try
    Print #0, "lines: "; count
    End count
End Sub
