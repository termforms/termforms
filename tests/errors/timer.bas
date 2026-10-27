' Errors test: Timers: a property a Timer does not have (it is never
' shown, so it has no place), an Interval that is not a number, and an
' event it does not raise.
Begin Form f
    Begin Timer tmr
        Left = 3
        Interval = "fast"
    End
End

Sub tmr_Click()
End Sub
