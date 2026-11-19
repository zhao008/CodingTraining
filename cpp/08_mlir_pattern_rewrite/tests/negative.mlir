// CHECK-LABEL: func.func @keep_two(
// CHECK: %[[T:[a-zA-Z0-9_]+]] = arith.muli
// CHECK-NEXT: return %[[T]] : i32
func.func @keep_two(%x: i32) -> i32 {
  %two = arith.constant 2 : i32
  %result = arith.muli %x, %two : i32
  return %result : i32
}

// CHECK-LABEL: func.func @keep_i64(
// CHECK: %[[W:[a-zA-Z0-9_]+]] = arith.muli
// CHECK-NEXT: return %[[W]] : i64
func.func @keep_i64(%x: i64) -> i64 {
  %one = arith.constant 1 : i64
  %result = arith.muli %x, %one : i64
  return %result : i64
}

// CHECK-LABEL: func.func @keep_vector(
// CHECK: %[[V:[a-zA-Z0-9_]+]] = arith.muli
// CHECK-NEXT: return %[[V]] : vector<2xi32>
func.func @keep_vector(
    %x: vector<2xi32>) -> vector<2xi32> {
  %one = arith.constant dense<1> : vector<2xi32>
  %result = arith.muli %x, %one : vector<2xi32>
  return %result : vector<2xi32>
}

// CHECK-LABEL: func.func @keep_float(
// CHECK: %[[F:[a-zA-Z0-9_]+]] = arith.mulf
// CHECK-NEXT: return %[[F]] : f32
func.func @keep_float(%x: f32) -> f32 {
  %one = arith.constant 1.0 : f32
  %result = arith.mulf %x, %one : f32
  return %result : f32
}
