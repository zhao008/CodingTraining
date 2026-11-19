// CHECK-LABEL: func.func @right_one(
// CHECK-SAME: %[[R:[a-zA-Z0-9_]+]]: i32
// CHECK-NOT: arith.muli
// CHECK: return %[[R]] : i32
// CHECK-NEXT: }
func.func @right_one(%x: i32) -> i32 {
  %one = arith.constant 1 : i32
  %result = arith.muli %x, %one : i32
  return %result : i32
}

// CHECK-LABEL: func.func @left_one(
// CHECK-SAME: %[[L:[a-zA-Z0-9_]+]]: i32
// CHECK-NOT: arith.muli
// CHECK: return %[[L]] : i32
// CHECK-NEXT: }
func.func @left_one(%x: i32) -> i32 {
  %one = arith.constant 1 : i32
  %result = arith.muli %one, %x : i32
  return %result : i32
}

// CHECK-LABEL: func.func @chain(
// CHECK-SAME: %[[C:[a-zA-Z0-9_]+]]: i32
// CHECK-NOT: arith.muli
// CHECK: return %[[C]] : i32
// CHECK-NEXT: }
func.func @chain(%x: i32) -> i32 {
  %one = arith.constant 1 : i32
  %a = arith.muli %x, %one : i32
  %b = arith.muli %one, %a : i32
  %c = arith.muli %b, %one : i32
  return %c : i32
}

// CHECK-LABEL: func.func @multiple_uses(
// CHECK-SAME: %[[M:[a-zA-Z0-9_]+]]: i32
// CHECK-NOT: arith.muli
// CHECK: %[[SUM:[a-zA-Z0-9_]+]] = arith.addi %[[M]], %[[M]] : i32
// CHECK-NEXT: return %[[SUM]] : i32
// CHECK-NEXT: }
func.func @multiple_uses(%x: i32) -> i32 {
  %one = arith.constant 1 : i32
  %a = arith.muli %x, %one : i32
  %sum = arith.addi %a, %a : i32
  return %sum : i32
}
