; CHECK: function=no_loop loops=0
; CHECK-NOT: loop header=

define i32 @no_loop(i32 %x) {
entry:
  %y = add i32 %x, 1
  ret i32 %y
}
