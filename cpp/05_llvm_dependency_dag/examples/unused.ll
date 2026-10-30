define i32 @unused(i32 %x) {
entry:
  %unused = mul i32 %x, 2
  %answer = add i32 %x, 1
  ret i32 %answer
}
