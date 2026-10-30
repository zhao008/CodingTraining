define i32 @vector_add(<4 x i32> %v) {
entry:
  %a = add <4 x i32> %v, %v
  ret i32 0
}
