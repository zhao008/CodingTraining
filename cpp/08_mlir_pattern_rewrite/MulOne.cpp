#include "MulOne.h"

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Pass/PassRegistry.h"
#include "mlir/Transforms/WalkPatternRewriteDriver.h"

#include <utility>

namespace {

struct MulOnePattern
    : public mlir::OpRewritePattern<mlir::arith::MulIOp> {
    using mlir::OpRewritePattern<
        mlir::arith::MulIOp>::OpRewritePattern;

    bool isConstantOne(mlir::Value value) const {
        auto constant = value.getDefiningOp<mlir::arith::ConstantOp>();
        if(!constant)
            return false;
        
        auto attr = mlir::dyn_cast<mlir::IntegerAttr>(constant.getValue());
        if(!attr)
            return false;
        
        return attr.getValue().isOne();
    }

    mlir::LogicalResult matchAndRewrite(
        mlir::arith::MulIOp op,
        mlir::PatternRewriter& rewriter) const override {

        // TODO 1：确认结果类型是标量 signless i32。
        //
        mlir::Type type = op.getType();
        auto intType = mlir::dyn_cast<mlir::IntegerType>(type);
        if( !intType ){
            return mlir::failure();
        }
        if( !( intType.isSignless() && (intType.getWidth()==32) ) ){
            return mlir::failure();
        }
        // TODO 2：检查左操作数是否为直接定义的整数常量 1。
        //
        if(isConstantOne(op.getLhs())){
            rewriter.replaceOp(op, op.getRhs());
            return mlir::success();
        }
        // TODO 3：检查右操作数是否为直接定义的整数常量 1。
        //
        if(isConstantOne(op.getRhs())){
            rewriter.replaceOp(op, op.getLhs());
            return mlir::success();
        }
        // TODO 4：匹配时通过 rewriter 替换整个乘法，
        //         然后返回 success。
        //
        // 未匹配时不修改任何 IR，返回 failure。

        return mlir::failure();
    }
};

struct MulOnePass
    : public mlir::PassWrapper<
          MulOnePass,
          mlir::OperationPass<mlir::func::FuncOp>> {

    MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(MulOnePass)

    llvm::StringRef getArgument() const override {
        return "training-mul-one";
    }

    llvm::StringRef getDescription() const override {
        return "Remove scalar i32 multiplication by one";
    }

    void runOnOperation() override {
        mlir::RewritePatternSet patterns(&getContext());
        patterns.add<MulOnePattern>(&getContext());

        mlir::FrozenRewritePatternSet frozen(
            std::move(patterns));

        mlir::walkAndApplyPatterns(
            getOperation().getOperation(), frozen);
    }
};

} // namespace

namespace training {

void registerMulOnePass() {
    mlir::PassRegistration<MulOnePass>();
}

} // namespace training
