//===----------------------------------------------------------------------===//
//
// Copyright (c) Microsoft Corporation, Meta Platforms.
// Licensed under the MIT license.
//
//===----------------------------------------------------------------------===//

#include "mlir/Conversion/ReconcileUnrealizedCasts/ReconcileUnrealizedCasts.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/SCF/Transforms/Patterns.h"
#include "mlir/IR/Attributes.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypeInterfaces.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/TypeRange.h"
#include "mlir/IR/Types.h"
#include "mlir/IR/Value.h"
#include "mlir/IR/ValueRange.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Support/LogicalResult.h"
#include "triton-shared/Analysis/OpFoldResultUtils.h"
#include "triton-shared/AnalysisStructured/PtrAnalysis.h"
#include "triton-shared/Conversion/TritonPtrToIndex/TritonPtrToIndex.h"
#include "triton-shared/Dialect/TritonStructured/IR/TritonStructuredDialect.h"

#include "triton/Dialect/Triton/IR/Dialect.h"

#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Bufferization/IR/Bufferization.h"
#include "mlir/Pass/PassManager.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"
#include "mlir/Transforms/OneToNTypeConversion.h"
#include "mlir/Transforms/Passes.h"
#include "triton/Dialect/Triton/IR/Types.h"

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/LogicalResult.h"
#include <cassert>
#include <optional>

#define DEBUG_TYPE "triton-ptr-to-index"

using namespace mlir;
using namespace triton;

#define GEN_PASS_CLASSES
#include "triton-shared/Conversion/TritonPtrToIndex/Passes.h.inc"

namespace {

// Type getIndexType() {
//   auto pointeeType = ptrType.getPointeeType();
//   if (auto shapedType = dyn_cast<ShapedType>(pointeeType)) {
//     return RankedTensorType::get(shapedType.getShape(),
//                                  IndexType::get(context));
//   }
//   return IndexType::get(context);
// }

class TritonTypeConverter : public TypeConverter {
public:
  TritonTypeConverter(MLIRContext *context) {
    addConversion([](Type type) { return type; });
    addConversion(
        [context](IntegerType type) { return IndexType::get(context); });
    addConversion([context](RankedTensorType tensorType)
                      -> std::optional<RankedTensorType> {
      if (isa<triton::PointerType, IntegerType>(tensorType.getElementType())) {
        return RankedTensorType::get(tensorType.getShape(),
                                     IndexType::get(context));
      }
      return std::nullopt;
    });
    addConversion([context](triton::PointerType ptrType) -> Type {
      return IndexType::get(context);
    });

    addSourceMaterialization([&](OpBuilder &builder, Type type,
                                 ValueRange inputs,
                                 Location loc) -> std::optional<Value> {
      return builder.create<UnrealizedConversionCastOp>(loc, type, inputs)
          ->getResult(0);
    });

    addTargetMaterialization([&](OpBuilder &builder, Type type,
                                 ValueRange inputs,
                                 Location loc) -> std::optional<Value> {
      // return builder.create<arith::ConstantOp>(loc,
      //                                          builder.getI64IntegerAttr(0));
      if (inputs.size() == 1 && isa<triton::PointerType>(inputs[0].getType())) {
        auto op = builder.create<UnrealizedConversionCastOp>(loc, type, inputs);
        op->setAttr("target-mat", UnitAttr::get(builder.getContext()));
        return op->getResult(0);
      }
      return builder.create<arith::IndexCastOp>(loc, type, inputs);
      // return std::nullopt;
    });

    addArgumentMaterialization([&](OpBuilder &builder, Type type,
                                   ValueRange inputs,
                                   Location loc) -> std::optional<Value> {
      auto op = builder.create<UnrealizedConversionCastOp>(loc, type, inputs);
      op->setAttr("arg-mat", UnitAttr::get(builder.getContext()));
      return op->getResult(0);
    });
  }
};

struct SplatConverter : public OpConversionPattern<triton::SplatOp> {
  using OpConversionPattern<triton::SplatOp>::OpConversionPattern;

  LogicalResult
  matchAndRewrite(triton::SplatOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    auto loc = op->getLoc();
    auto src = adaptor.getSrc();

    // tt.splat only takes tensor of integer or integer, so we have to index
    // cast back to the original type
    auto i64SrcTargetType =
        getTypeConverter()
            ->convertType(src.getType())
            .replace([&](IndexType t) {
              return IntegerType::get(rewriter.getContext(), 64);
            });

    auto i64ResTargetType =
        getTypeConverter()
            ->convertType(op.getResult().getType())
            .replace([&](IndexType t) {
              return IntegerType::get(rewriter.getContext(), 64);
            });

    auto castSrc =
        rewriter.create<arith::IndexCastOp>(loc, i64SrcTargetType, src);

    auto splat =
        rewriter.create<triton::SplatOp>(loc, i64ResTargetType, castSrc);

    // index cast back again

    auto replacement = rewriter.create<arith::IndexCastOp>(
        loc, getTypeConverter()->convertType(op.getResult().getType()), splat);

    rewriter.replaceOp(op, replacement);
    return success();
  }
};

struct BroadcastConverter : public OpConversionPattern<triton::BroadcastOp> {
  using OpConversionPattern<triton::BroadcastOp>::OpConversionPattern;

  LogicalResult
  matchAndRewrite(triton::BroadcastOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    auto loc = op->getLoc();
    auto src = adaptor.getSrc();

    // tt.splat only takes tensor of integer or integer, so we have to index
    // cast back to the original type
    auto i64SrcTargetType =
        getTypeConverter()
            ->convertType(src.getType())
            .replace([&](IndexType t) {
              return IntegerType::get(rewriter.getContext(), 64);
            });

    auto i64ResTargetType =
        getTypeConverter()
            ->convertType(op.getResult().getType())
            .replace([&](IndexType t) {
              return IntegerType::get(rewriter.getContext(), 64);
            });

    auto castSrc =
        rewriter.create<arith::IndexCastOp>(loc, i64SrcTargetType, src);

    auto splat =
        rewriter.create<triton::SplatOp>(loc, i64ResTargetType, castSrc);

    // index cast back again

    auto replacement = rewriter.create<arith::IndexCastOp>(
        loc, getTypeConverter()->convertType(op.getResult().getType()), splat);

    rewriter.replaceOp(op, replacement);
    return success();
  }
};

struct LoadConverter : public OpConversionPattern<triton::LoadOp> {
  using OpConversionPattern<triton::LoadOp>::OpConversionPattern;

  LogicalResult
  matchAndRewrite(triton::LoadOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    auto loc = op->getLoc();
    auto ptrType = op.getPtr().getType();

    auto cast =
        rewriter
            .create<UnrealizedConversionCastOp>(loc, ptrType, adaptor.getPtr())
            ->getResult(0);

    auto replacement =
        dyn_cast<triton::LoadOp>(rewriter.clone(*op.getOperation()));
    // replacement
    replacement.getPtrMutable().set(cast);
    rewriter.replaceOp(op, replacement);
    return success();
  }
};

struct StoreConverter : public OpConversionPattern<triton::StoreOp> {
  using OpConversionPattern<triton::StoreOp>::OpConversionPattern;

  LogicalResult
  matchAndRewrite(triton::StoreOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    auto loc = op->getLoc();
    auto ptrType = op.getPtr().getType();

    auto cast =
        rewriter
            .create<UnrealizedConversionCastOp>(loc, ptrType, adaptor.getPtr())
            ->getResult(0);

    auto replacement =
        dyn_cast<triton::StoreOp>(rewriter.clone(*op.getOperation()));
    // replacement
    replacement.getPtrMutable().set(cast);
    rewriter.replaceOp(op, replacement);
    return success();
  }
};

struct AddPtrConverter : public OpConversionPattern<triton::AddPtrOp> {
  using OpConversionPattern<triton::AddPtrOp>::OpConversionPattern;

  Value getIndexValue(Value v, Location loc, OpBuilder &b) const {
    auto targetType = getTypeConverter()->convertType(v.getType());
    if (targetType == v.getType()) {
      return v;
    }
    return b.create<arith::IndexCastOp>(loc, targetType, v);
  }

  LogicalResult
  matchAndRewrite(triton::AddPtrOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {

    auto func = op->getParentOfType<triton::FuncOp>();
    auto loc = op->getLoc();

    auto off = getIndexValue(adaptor.getOffset(), loc, rewriter);
    auto prevOff = getIndexValue(adaptor.getPtr(), loc, rewriter);
    auto accumulatedOff = rewriter.create<arith::AddIOp>(loc, prevOff, off);
    rewriter.replaceOp(op, accumulatedOff);

    return success();
  }
};

class TritonPtrToIndexPass : public TritonPtrToIndexBase<TritonPtrToIndexPass> {

  void bfs(Operation *op) {
    std::queue<std::pair<Value, Value>> q;
    DenseSet<Value> visited;

    op->walk([&q, &visited](UnrealizedConversionCastOp op) {
      if (op->hasAttr("target-mat")) {
        auto value = op->getResult(0);
        q.push({value, op.getInputs()[0]});
        visited.insert(value);
      }
    });

    // // Consider ptrs used directly without addptr
    // op->walk([&q, &visited](triton::FuncOp op) {
    //   for (auto arg : op.getArguments()) {
    //     if (isa<triton::PointerType>(arg.getType())) {
    //       q.push({arg, arg});
    //     }
    //   }
    // });

    while (!q.empty()) {
      auto [v, arg] = q.front();
      // llvm::dbgs() << "visiting: \n";
      // v.dump();
      q.pop();
      for (auto user : v.getUsers()) {
        // scf.for is a special case. We have 2 set of values to consider:
        // - iter-args
        // - loop results
        // for every init arg that originates from a
        // `tts.get_structured_state` op, its corresponding iter-arg and loop
        // result will also be considered "maybeStructured".
        if (auto castOp = dyn_cast<UnrealizedConversionCastOp>(user)) {
          castOp->dump();
          // assert(castOp.getInputs().size() == 1);
          // assert(castOp->getResults().size() == 1);
          for (auto user : castOp->getUsers()) {
            auto isLoadStore = llvm::isa<triton::LoadOp, triton::StoreOp>(user);
            if (!isLoadStore) {
              user->dump();
            }
          }

          // assert(expr)
          // castOp->dump();
          castOp->setOperands({arg, castOp->getOperands()[0]});
          // castOp.setOperand(1, arg);
          // castOp->dump();

        } else if (auto forOp = dyn_cast<scf::ForOp>(user)) {
          auto it = llvm::find(forOp.getInitArgs(), v);
          // assert(0);

          if (it == forOp.getInitArgs().end()) {
            continue;
          }

          // assert(0);
          auto argIndex = std::distance(forOp.getInitArgs().begin(), it);
          auto iterArg = forOp.getRegionIterArg(argIndex);
          auto tiedLoopRes = forOp.getTiedLoopResult(iterArg);

          SmallVector<Value> neighbors{iterArg, tiedLoopRes};
          for (auto neighbor : neighbors) {
            if (!visited.contains(neighbor)) {
              visited.insert(neighbor);
              q.push({neighbor, arg});
            }
          }

        } else {
          for (auto res : user->getResults()) {
            if (res.getType() != v.getType()) {
              // continue;
            }
            if (!visited.contains(res)) {
              visited.insert(res);
              q.push({res, arg});
            }
          }
        }
      }
    }

    op->walk([&q, &visited](UnrealizedConversionCastOp cast) {
      if (cast->hasAttr("target-mat")) {
        OpBuilder b(cast);
        cast.getResult(0).replaceAllUsesWith(b.create<arith::ConstantOp>(
            cast->getLoc(), cast.getResult(0).getType(), b.getIndexAttr(0)));

        // if (cast.getResult(0).getType().isIndex()) {
        //   cast.getResult(0).replaceAllUsesWith(b.create<arith::ConstantOp>(
        //       cast->getLoc(), cast.getResult(0).getType(),
        //       b.getIndexTensorAttr({0})));
        // } else {
        //   cast.getResult(0).replaceAllUsesWith(b.create<arith::ConstantOp>(
        //       cast->getLoc(), cast.getResult(0).getType(),
        //       b.getIndexAttr(0)));
        // }

        cast->erase();
      }
    });
  }

public:
  void getDependentDialects(DialectRegistry &registry) const override {
    registry
        .insert<arith::ArithDialect, math::MathDialect, affine::AffineDialect,
                scf::SCFDialect, tensor::TensorDialect, triton::TritonDialect,
                tts::TritonStructuredDialect>();
  }

  void convertLoop() {
    auto moduleOp = getOperation();
    RewritePatternSet patterns(&getContext());
    ConversionTarget target(getContext());

    TritonTypeConverter converter(&getContext());
    scf::populateSCFStructuralTypeConversionsAndLegality(converter, patterns,
                                                         target);

    if (failed(applyPartialConversion(moduleOp, target, std::move(patterns)))) {
      signalPassFailure();
    }
  }

  void runOnOperation() override {
    auto moduleOp = getOperation();

    RewritePatternSet patterns(&getContext());
    ConversionTarget target(getContext());

    target.addLegalDialect<
        func::FuncDialect, arith::ArithDialect, math::MathDialect,
        affine::AffineDialect, scf::SCFDialect, cf::ControlFlowDialect,
        tensor::TensorDialect, bufferization::BufferizationDialect>();

    target.addLegalOp<ModuleOp>();

    target.addIllegalOp<triton::AddPtrOp>();

    target.addDynamicallyLegalOp<UnrealizedConversionCastOp>([](Operation *op) {
      auto resType = op->getResultTypes()[0];
      return !isa<triton::PointerType>(resType);
    });

    target.addDynamicallyLegalOp<triton::SplatOp>([](triton::SplatOp op) {
      auto resType = op.getResult().getType();
      if (auto shapedType = dyn_cast<ShapedType>(resType)) {
        return !isa<triton::PointerType>(shapedType.getElementType());
      }
      return !isa<triton::PointerType>(resType);
    });

    target.addDynamicallyLegalOp<triton::BroadcastOp>(
        [](triton::BroadcastOp op) {
          auto resType = op.getResult().getType();
          if (auto shapedType = dyn_cast<ShapedType>(resType)) {
            return !isa<triton::PointerType>(shapedType.getElementType());
          }
          return !isa<triton::PointerType>(resType);
        });

    TritonTypeConverter converter(&getContext());
    // scf::populateSCFStructuralTypeConversionsAndLegality(converter, patterns,
    //                                                      target);

    // patterns.add<AddPtrConverter>(converter, &getContext());

    patterns.add<AddPtrConverter, SplatConverter, BroadcastConverter>(
        converter, &getContext());
    scf::populateSCFStructuralTypeConversionsAndLegality(converter, patterns,
                                                         target);

    if (failed(applyPartialConversion(moduleOp, target, std::move(patterns)))) {
      signalPassFailure();
    }

    // not sure why we cannot run this together
    // convertLoop();

    PassManager pm(&getContext(), moduleOp.getOperationName());
    pm.addPass(createReconcileUnrealizedCastsPass());
    if (failed(runPipeline(pm, getOperation()))) {
      signalPassFailure();
    }

    moduleOp->dump();

    bfs(moduleOp.getOperation());
  }
};
} // namespace

std::unique_ptr<OperationPass<ModuleOp>> triton::createTritonPtrToIndexPass() {
  return std::make_unique<TritonPtrToIndexPass>();
}
