define i32 @varargs(i32 %x, ...) {
entry:
  %a = add i32 %x, 1
  ret i32 %a
}
