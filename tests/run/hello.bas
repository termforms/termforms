' hello.bas -- the TermForms vertical slice: one form, a TextBox, a
' CommandButton and a Click handler.
'
' Build and run:   tfc hello.bas && ./hello
' Keys:            Tab moves between the controls, Enter or Space presses
'                  the button, Ctrl+Q quits.

Begin Form frmHello
    Caption = "Hello"
    Width   = 40
    Height  = 8
    Begin TextBox txtName
        Left  = 2
        Top   = 1
        Width = 34
    End
    Begin CommandButton cmdGreet
        Caption = "&Greet"
        Left    = 2
        Top     = 3
        Width   = 12
    End
End

Dim greetings As Integer

Sub cmdGreet_Click()
    Dim who As String
    who = txtName.Text
    If who = "" Then who = "world"
    greetings = greetings + 1
    txtName.Text = "Hello, " & who & "! (" & greetings & ")"
End Sub
