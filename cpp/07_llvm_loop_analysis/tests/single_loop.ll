; CHECK: function=single_loop loops=1
; CHECK-NEXT: loop header=%header depth=1 blocks=2
; CHECK-NOT: loop header=

define void @single_loop(i32 %n) {
entry:
  br label %header

header:
  %i = phi i32 [ 0, %entry ], [ %i.next, %body ]
  %continue = icmp slt i32 %i, %n
  br i1 %continue, label %body, label %exit

body:
  %i.next = add i32 %i, 1
  br label %header

exit:
  ret void
}
