define i32 @repeated_use(i32 %x) {
entry:
  %a = add i32 %x, 1
  %b = mul i32 %a, %a
  ret i32 %b
}
