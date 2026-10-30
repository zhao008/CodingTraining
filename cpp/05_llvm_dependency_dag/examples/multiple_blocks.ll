define i32 @multiple_blocks(i32 %x) {
entry:
  br label %exit

exit:
  ret i32 %x
}
