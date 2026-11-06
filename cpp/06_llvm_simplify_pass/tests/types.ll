; README 第 8 节：不应修改的类型，以及两个补充案例。

; CHECK-LABEL: define i64 @keep_i64(
; CHECK: %a = add i64 %x, 0
; CHECK-NEXT: ret i64 %a
define i64 @keep_i64(i64 %x) {
entry:
  %a = add i64 %x, 0
  ret i64 %a
}

; CHECK-LABEL: define float @keep_float(
; CHECK: %a = fadd float %x,
; CHECK-NEXT: ret float %a
define float @keep_float(float %x) {
entry:
  %a = fadd float %x, 0.000000e+00
  ret float %a
}

; CHECK-LABEL: define <2 x i32> @keep_vector(
; CHECK: %a = add <2 x i32> %x, zeroinitializer
; CHECK-NEXT: ret <2 x i32> %a
define <2 x i32> @keep_vector(<2 x i32> %x) {
entry:
  %a = add <2 x i32> %x, zeroinitializer
  ret <2 x i32> %a
}

; 补充案例 1：多个基本块，每个块里都有可消除的 add。
; CHECK-LABEL: define i32 @multi_block(
; CHECK-NOT: add i32
; CHECK: br i1 %c, label %then, label %else
; CHECK-NOT: add i32
; CHECK: ret i32 %x
; CHECK-NOT: add i32
; CHECK: ret i32 %y
; CHECK-NEXT: }
define i32 @multi_block(i32 %x, i32 %y, i1 %c) {
entry:
  %a = add i32 %x, 0
  br i1 %c, label %then, label %else
then:
  ret i32 %a
else:
  %b = add i32 0, %y
  ret i32 %b
}

; 补充案例 2：带 nsw / nuw 的标量 i32 加零，同样消除。
; CHECK-LABEL: define i32 @with_flags(
; CHECK-NOT: add
; CHECK: ret i32 %x
; CHECK-NEXT: }
define i32 @with_flags(i32 %x) {
entry:
  %a = add nsw i32 %x, 0
  %b = add nuw i32 0, %a
  %c = add nuw nsw i32 %b, 0
  ret i32 %c
}
