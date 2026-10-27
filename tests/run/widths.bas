' Run test: wide characters take two cells. A Label, a button and a
' ListBox item in Chinese and Japanese, an emoji, and a combining accent
' (not drawn: the cell holds the letter); a TextBox narrower than its text
' scrolls by whole characters and the caret moves over one wide character
' at a time; a wide character that would cross the box's edge is left out.
' Len counts code points, not cells.
Begin Form frmMain
    Caption = "宽字符"
    Width = 40
    Height = 12
    Begin Label lblWide
        Caption = "x"
        Width = 30
    End
    Begin TextBox txtWide
        Top = 1
        Width = 7
    End
    Begin CommandButton cmdGo
        Caption = "&Go 走"
        Top = 2
        Width = 12
    End
    Begin ListBox lstWide
        Top = 3
        Width = 13
        Height = 3
    End
    Begin Label lblLog
        Top = 7
        Width = 38
        Height = 2
    End
End

Sub Form_Load()
    lblWide.Caption = "中文 日本語 😀 café!"
    lstWide.AddItem "一二三四五六七八"
    txtWide.Text = "ab中文字"
End Sub

Sub cmdGo_Click()
    lblLog.Caption = "len=" & Len(txtWide.Text) & " sel=" & txtWide.SelStart & " [" & txtWide.Text & "]"
End Sub
