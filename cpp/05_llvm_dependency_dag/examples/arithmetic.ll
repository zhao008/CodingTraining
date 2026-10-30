define i32 @arithmetic(i32 %x, i32 %y) {
entry:
  %a = add i32 %x, 1
  %b = mul i32 %y, 2
  %c = add i32 %a, %b
  %d = mul i32 %a, 3
  %e = add i32 %c, %d
  ret i32 %e
}
