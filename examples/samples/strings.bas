' strings.bas -- Strings count characters (code points), from 1: Len, Mid$,
' Left$, Right$, InStr, Replace, case, trimming; numbers to text and back;
' the Mid$ statement, which replaces characters in place.
'
' Build and run:   tfc strings.bas && ./strings
Begin Form frmSample
    Caption = "Strings"
    Width = 70
    Height = 20
    Begin ListBox lstOut
        Left = 1
        Width = 66
        Height = 17
    End
End

' Each line of the sample's results.
Sub Note(text As String)
    lstOut.AddItem text
End Sub

Sub Form_Load()
    Dim s As String = "Hello, wörld"
    Note "s = """ & s & """: Len " & Len(s) & " characters, LenB " & LenB(s) & " bytes"
    Note "Left$(s, 5) = " & Left$(s, 5) & ", Right$(s, 5) = " & Right$(s, 5)
    Note "Mid$(s, 8, 3) = " & Mid$(s, 8, 3) & ", InStr(s, ""w"") = " & InStr(s, "w")
    Note "Replace: " & Replace(s, "Hello", "Goodbye")
    Note "UCase$: " & UCase$(s) & " (A-Z only for now)"
    Note "Trim$(""  padded  "") = [" & Trim$("  padded  ") & "]"
    Note "StrReverse: " & StrReverse("stressed")
    Dim n As Integer = 42
    Note "CStr(42) = """ & CStr(n) & """, Str$(42) = """ & Str$(n) & """ (a space for the sign)"
    Note "Val(""3.5 apples"") = " & Val("3.5 apples")
    Dim code As String = "ABC-123"
    Mid$(code, 5, 3) = "999"
    Note "after Mid$(code, 5, 3) = ""999"": " & code
    Note "Chr$(65) & Asc(""B""): " & Chr$(65) & Asc("B")
End Sub
