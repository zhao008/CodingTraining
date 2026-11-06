; CHECK-LABEL: define i32 @right_zero(
; CHECK-NOT: add i32
; CHECK: ret i32 %x
; CHECK-NEXT: }
define i32 @right_zero(i32 %x) {
entry:
  %a = add i32 %x, 0
  ret i32 %a
}

; CHECK-LABEL: define i32 @left_zero(
; CHECK-NOT: add i32
; CHECK: ret i32 %x
; CHECK-NEXT: }
define i32 @left_zero(i32 %x) {
entry:
  %a = add i32 0, %x
  ret i32 %a
}

; CHECK-LABEL: define i32 @chain(
; CHECK-NOT: add i32
; CHECK: ret i32 %x
; CHECK-NEXT: }
define i32 @chain(i32 %x) {
entry:
  %a = add i32 %x, 0
  %b = add i32 0, %a
  %c = add i32 %b, 0
  ret i32 %c
}

; CHECK-LABEL: define i32 @multiple_uses(
; CHECK-NOT: %a =
; CHECK: %b = mul i32 %x, %x
; CHECK-NEXT: %c = add i32 %x, %b
; CHECK-NEXT: ret i32 %c
define i32 @multiple_uses(i32 %x) {
entry:
  %a = add i32 %x, 0
  %b = mul i32 %a, %a
  %c = add i32 %a, %b
  ret i32 %c
}

; CHECK-LABEL: define i32 @unchanged(
; CHECK: %a = add i32 %x, 1
; CHECK-NEXT: %b = mul i32 %a, 0
; CHECK-NEXT: ret i32 %b
define i32 @unchanged(i32 %x) {
entry:
  %a = add i32 %x, 1
  %b = mul i32 %a, 0
  ret i32 %b
}
