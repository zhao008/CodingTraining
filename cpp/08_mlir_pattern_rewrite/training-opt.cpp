#include "MulOne.h"

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/DialectRegistry.h"
#include "mlir/Tools/mlir-opt/MlirOptMain.h"

int main(int argc, char** argv) {
    training::registerMulOnePass();

    mlir::DialectRegistry registry;
    registry.insert<
        mlir::arith::ArithDialect,
        mlir::func::FuncDialect>();

    return mlir::asMainReturnCode(
        mlir::MlirOptMain(
            argc,
            argv,
            "CodingTraining MLIR optimizer\n",
            registry));
}
