' Errors test: controls: a control inside one that is not a Frame, a
' property a control does not have, the wrong type for Value (a CheckBox's
' is an Integer, an OptionButton's a Boolean), and an event a Frame lacks.
Begin Form f
    Begin CommandButton b
        Begin Label inner
        End
    End
    Begin Label lbl
        Value = 1
    End
    Begin CheckBox chk
        Value = True
    End
    Begin OptionButton opt
        Value = 1
    End
    Begin Frame fra
    End
End

Sub fra_Click()
End Sub
