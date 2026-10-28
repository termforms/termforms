' Golden test: Declares become extern prototypes with C's types (no
' definitions); a ByVal String goes as tf_str_cstr, a ByRef String through
' a buffer written back after the call, a CLong through a checked
' conversion before it; a String result is copied; the C name is the
' Alias.
Begin Form frmMain
    Caption = "Declare"
End

Declare Function strlen Lib "" (ByVal s As String) As CSizeT
Declare Sub get_name Lib "" Alias "fill_name" (ByRef buffer As String, ByVal size As CLong)
Declare Function version Lib "sqlite3" Alias "sqlite3_libversion" () As String

Sub Form_Load()
    Dim buffer As String = Space$(8)
    get_name buffer, 8
    Me.Caption = version() & CStr(strlen(buffer))
End Sub
