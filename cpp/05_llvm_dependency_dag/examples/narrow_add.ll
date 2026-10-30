define i32 @narrow_add(i32 %x) {
entry:
  %a = add i8 1, 2
  ret i32 %x
}
