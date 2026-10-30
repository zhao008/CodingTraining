define i32 @first_ok(i32 %x) {
entry:
  %a = add i32 %x, 1
  %b = mul i32 %a, 2
  ret i32 %b
}

define i32 @then_fail(i32 %x) {
entry:
  %a = add i32 %x, 1
  %b = sub i32 %a, 2
  ret i32 %b
}

define i32 @then_empty(i32 %x) {
entry:
  ret i32 %x
}

define i32 @finally_ok(i32 %x) {
entry:
  %a = mul i32 %x, 3
  ret i32 %a
}
