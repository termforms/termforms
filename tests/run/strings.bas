' Run test: the string functions, with and without $, positions from 1
' counted in characters (UTF-8 too), optional arguments (Mid$'s length,
' InStr's start, Split's delimiter), Split into an array and in For Each,
' Join, and errors 5 (a bad argument) and 14 (a string too long) caught.
Begin Form f
    Caption = "Strings"
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

Sub Form_Load()
    Dim s As String, parts() As String, p As String, n As Integer, euro As String
    s = "Hello, World"
    t1.Text = Mid$(s, 8) & "|" & Mid(s, 1, 5) & "|" & Left$(s, 2) & Right(s, 3) & "|" & _
        InStr(s, "o") & InStr(6, s, "o") & InStr(s, "z") & InStrRev(s, "o") & "|" & Len(s)

    euro = Chr$(8364) & "uro"
    t2.Text = UCase$(s) & " " & LCase(Left$(s, 1)) & " [" & Trim$("  x  ") & "][" & _
        LTrim("  y") & "][" & RTrim$("z  ") & "] " & euro & Len(euro) & LenB(euro) & Asc(euro)

    t3.Text = Replace(s, "o", "0") & " " & StrReverse("ab" & euro) & " " & Space$(2) & _
        String$(3, "-") & String$(2, euro)

    parts = Split("a,b,,c", ",")
    p = ""
    For Each s In parts
        p = p & "<" & s & ">"
    Next
    For Each s In Split("one two")
        p = p & s & ";"
    Next
    t4.Text = p & " " & UBound(parts) & " " & Join(parts, "+") & " " & Join(parts)

    s = ""
    Try
        s = Mid$("abc", 0, 1)
    Catch When Err.Number = 5
        s = "5"
    End Try
    Try
        n = Asc("")
    Catch
        s = s & " " & Err.Number
    End Try
    Try
        p = String$(2000000000, "€")  ' 6 billion bytes: refused before allocating
    Catch
        s = s & " " & Err.Number
    End Try
    t5.Text = s
End Sub
