' Errors test: menus: a third level, a control in a Menu, a Menu in a
' Frame, a check mark or shortcut on the bar, Shortcut texts the runtime
' cannot use (keys a terminal or the forms take), and Left on a Menu.
Begin Form f
    Begin Menu mnuFile
        Caption = "&File"
        Checked = True
        Begin Menu mnuOpen
            Caption = "&Open"
            Begin Menu mnuDeep
            End
        End
        Begin Label lbl
        End
        Begin Menu mnuA
            Shortcut = "Ctrl+M"
        End
        Begin Menu mnuB
            Shortcut = "F10"
        End
        Begin Menu mnuC
            Shortcut = "Ctrl+F4"
        End
        Begin Menu mnuD
            Shortcut = "Ctrl+Shift+X"
        End
        Begin Menu mnuE
            Shortcut = "F13"
        End
        Begin Menu mnuF
            Shortcut = "Ctrl+B"
        End
        Begin Menu mnuG
            Shortcut = "shift+ctrl+f12"
            Left = 2
        End
    End
    Begin Menu mnuBar
        Shortcut = "Ctrl+Q"
    End
    Begin Frame fra
        Begin Menu mnuInFrame
        End
    End
End
