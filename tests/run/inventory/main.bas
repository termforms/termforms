' The form module of the inventory project (run test).
Begin Form frmMain
    Caption = "Inventory"
    Width = 40
    Height = 6
    Begin TextBox txtTotal
        Width = 30
    End
End

Sub Form_Load()
    Add 3
    Add 4
    txtTotal.Text = "total " & total & " from " & stock.Count()
End Sub
