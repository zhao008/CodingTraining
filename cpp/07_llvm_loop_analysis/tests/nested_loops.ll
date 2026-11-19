; CHECK: function=nested loops=2
; CHECK-NEXT: loop header=%outer.header depth=1 blocks=5
; CHECK-NEXT: loop header=%inner.header depth=2 blocks=2
; CHECK-NOT: loop header=

define void @nested(i32 %n, i32 %m) {
entry:
  br label %outer.header

outer.header:
  %i = phi i32 [ 0, %entry ], [ %i.next, %outer.latch ]
  %outer.continue = icmp slt i32 %i, %n
  br i1 %outer.continue, label %inner.preheader, label %exit

inner.preheader:
  br label %inner.header

inner.header:
  %j = phi i32 [ 0, %inner.preheader ], [ %j.next, %inner.body ]
  %inner.continue = icmp slt i32 %j, %m
  br i1 %inner.continue, label %inner.body, label %outer.latch

inner.body:
  %j.next = add i32 %j, 1
  br label %inner.header

outer.latch:
  %i.next = add i32 %i, 1
  br label %outer.header

exit:
  ret void
}
