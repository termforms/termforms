' Run test: a program of three modules (see modules.with): Public and
' Private names, a module's Public variable, Const, Type and Function used
' from another, a name Public in two modules chosen by qualifying it, the
' same Private name in two modules kept apart, a Sub of a module called
' through its name, a module's code setting a control as frmMain.txtOut,
' a module named in any case (module names ignore case, as file names do
' on Windows and macOS), and an error raised in another module, caught.
Begin Form frmMain
    Caption = "Modules"
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
End

Dim hidden As Integer         ' Private to this module; stock has its own

Sub Form_Load()
    Dim p As Item
    hidden = 1
    AddStock "bolt", 5
    AddStock "nut", 7
    p = Lookup("nut")
    t1.Text = total & " " & p.Name & p.Qty & " " & MAX_ITEMS & " " & hidden & Stock.Hidden()
    t2.Text = stock.Label() & " " & REPORT.Label() & " " & Twice(4)  ' module names ignore case
    report.Show "shown"
    Try
        AddStock "x", -1
    Catch
        t4.Text = Err.Number & " " & Err.Description
    End Try
End Sub
