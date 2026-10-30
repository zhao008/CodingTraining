define i32 @unsupported(i32 %x) {
entry:
  %a = add i32 %x, 1
  %b = sub i32 %a, 2
  ret i32 %b
}
