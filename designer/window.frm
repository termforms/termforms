' The designer's own form. Form_Resize fits it to the terminal and lays
' out the panels; the places here are for a 100 x 30 terminal.
Begin Form frmDesigner
    Caption = "TermForms Designer"
    Width = 100
    Height = 30
    Begin Menu mnuFile
        Caption = "&File"
        Begin Menu mnuOpen
            Caption = "&Open..."
            Shortcut = "Ctrl+O"
        End
        Begin Menu mnuSave
            Caption = "&Save"
            Shortcut = "Ctrl+S"
        End
        Begin Menu mnuToFrm
            Caption = "&Move form to .frm"
        End
        Begin Menu mnuFileSep
            Caption = "-"
        End
        Begin Menu mnuExit
            Caption = "E&xit"
            Shortcut = "Ctrl+Q"
        End
    End
    Begin Menu mnuEdit
        Caption = "&Edit"
        Begin Menu mnuUndo
            Caption = "&Undo"
            Shortcut = "Ctrl+Z"
        End
        Begin Menu mnuRedo
            Caption = "&Redo"
            Shortcut = "Ctrl+Y"
        End
        Begin Menu mnuDelete
            Caption = "&Delete control"
        End
    End
    Begin Menu mnuView
        Caption = "&View"
        Begin Menu mnuTabOrder
            Caption = "&Tab order"
            Shortcut = "F6"
        End
        Begin Menu mnuCode
            Caption = "&Code"
            Shortcut = "F7"
        End
    End
    Begin Menu mnuAdd
        Caption = "&Add"
        Begin Menu mnuAddLabel
            Caption = "&Label"
        End
        Begin Menu mnuAddTextBox
            Caption = "&TextBox"
        End
        Begin Menu mnuAddCommandButton
            Caption = "&CommandButton"
        End
        Begin Menu mnuAddCheckBox
            Caption = "C&heckBox"
        End
        Begin Menu mnuAddOptionButton
            Caption = "&OptionButton"
        End
        Begin Menu mnuAddFrame
            Caption = "&Frame"
        End
        Begin Menu mnuAddListBox
            Caption = "L&istBox"
        End
        Begin Menu mnuAddComboBox
            Caption = "Co&mboBox"
        End
        Begin Menu mnuAddTimer
            Caption = "Time&r"
        End
    End
    Begin Menu mnuHelp
        Caption = "&Help"
        Begin Menu mnuKeys
            Caption = "&Keys..."
            Shortcut = "F1"
        End
    End
    Begin Label lblTools
        Caption = "T&oolbox"
        Width = 17
    End
    Begin ListBox lstTools
        Top = 1
        Width = 17
        Height = 11
    End
    Begin Label lblTimers
        Caption = "Ti&mers"
        Top = 12
        Width = 17
    End
    Begin ListBox lstTimers
        Top = 13
        Width = 17
        Height = 4
    End
    Begin Label lblForm
        Top = 18
        Width = 17
    End
    Begin CommandButton cmdTabOrder
        Caption = "Tab or&der"
        Top = 22
        Width = 13
    End
    Begin FormView fvForm
        Left = 18
        Width = 50
        Height = 24
    End
    Begin Label lblCtl
        Caption = "Co&ntrol"
        Left = 69
        Width = 29
    End
    Begin ComboBox cboCtl
        Left = 69
        Top = 1
        Width = 29
        Style = 2
    End
    Begin Label lblProps
        Caption = "P&roperties"
        Left = 69
        Top = 2
        Width = 29
    End
    Begin ListBox lstProps
        Left = 69
        Top = 3
        Width = 29
        Height = 12
    End
    Begin Label lblValue
        Left = 69
        Top = 15
        Width = 29
    End
    Begin TextBox txtValue
        Left = 69
        Top = 16
        Width = 29
    End
    Begin ComboBox cboValue
        Left = 69
        Top = 16
        Width = 29
        Style = 2
        Visible = False
    End
    Begin Label lblEvents
        Caption = "Even&ts (* written)"
        Left = 69
        Top = 17
        Width = 29
    End
    Begin ListBox lstEvents
        Left = 69
        Top = 18
        Width = 29
        Height = 6
    End
    Begin Label lblStatus
        Top = 25
        Width = 98
    End
    Begin Label lblKeys
        Top = 26
        Width = 98
    End
    Begin FormView fvScratch
        Width = 1
        Height = 1
        Visible = False
    End
End
