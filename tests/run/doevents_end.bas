' Run test: a program that runs its own loop, DoEvents in it, from Sub
' Main. The key script types into the TextBox, then runs out inside
' DoEvents, which ends the program (as it ends the event loop), showing
' the screen as it was; the loop never ends by itself.
Begin Form frmMain
    Caption = "Loop"
    Width = 30
    Height = 6
    Begin TextBox txtA
        Width = 10
    End
End

Sub Main()
    frmMain.Show
    Do
        DoEvents
    Loop
End Sub
