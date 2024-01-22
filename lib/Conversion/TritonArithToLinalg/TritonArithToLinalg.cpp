//===----------------------------------------------------------------------===//
//
// Copyright (c) Microsoft Corporation, Meta Platforms.
// Licensed under the MIT license.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/SmallVectorExtras.h"
#include "llvm/ADT/TypeSwitch.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/MathExtras.h"

#include "triton-shared/Conversion/TritonArithToLinalg/ConversionPatterns.hpp"
#include "triton-shared/Conversion/TritonArithToLinalg/TritonArithToLinalg.h"

using namespace mlir;
using namespace triton;

#define GEN_PASS_CLASSES
#include "triton-shared/Conversion/TritonArithToLinalg/Passes.h.inc"

void mlir::triton::populateTritonArithToLinalgCanonicalizationPatterns(
    RewritePatternSet &patterns) {
  patterns.add<MinMaxConverter<arith::CmpFOp>, MinMaxConverter<arith::CmpIOp>>(
      patterns.getContext());
}

void mlir::triton::populateTritonArithToLinalgConversionPatterns(
    bool pidsToFuncArgs, bool addptrToLinalg, bool assertToCf,
    RewritePatternSet &patterns) {

  if (pidsToFuncArgs) {
    patterns.add<GetProgramIDConverter, GetNumProgramsConverter>(
        patterns.getContext());
  }
  if (addptrToLinalg) {
    patterns.add<AddPtrConverter>(patterns.getContext());
  }
  if (assertToCf) {
    patterns.add<AssertConverter>(patterns.getContext());
  }
  patterns.add<BroadcastConverter>(patterns.getContext());
  patterns.add<TransposeConverter>(patterns.getContext());
  patterns.add<MakeRangeConverter>(patterns.getContext());
  patterns.add<ExpandDimsConverter>(patterns.getContext());
  patterns.add<BitcastConverter>(patterns.getContext());
  patterns.add<MatmulConverter>(patterns.getContext());
  patterns.add<SplatConverter>(patterns.getContext());
  patterns.add<DenseConstantConverter>(patterns.getContext());
  patterns.add<CumSumConverter>(patterns.getContext());
  patterns.add<ReshapeConverter>(patterns.getContext());

  populateExternElementwiseOpToMLIROps(patterns);

  // Reduce converters
  // Triton's reduce op is idential to linalg.reduce op, so we can clone
  // `tt.reduce` body to `linalg.reduce`. Unfortunately, we still need to
  // perform pattern matching to know what reduce ops we are dealing with
  // so that we know how to initialize the initial reduce values correctly.
  //
  // We can do this in a generic way without pattern matching by always using
  // the first elements along the reduction axis and perform the reduction on
  // the remaining elements. However, this results in creatings sub-tensors that
  // aren't always multiple of 2s, which are sub-optimal for certain hardwares.
  patterns.add<ArgMinConverter>(patterns.getContext());
  patterns.add<ArgMaxConverter>(patterns.getContext());
  patterns.add<ReduceConverter>(patterns.getContext());

  // Note: the ordering here matters!
  // Incorrect ordering or as lower PatternBenefit will result in element-wise
  // meta ops being converted to linalg.generic ops.
  linalg::populateElementwiseToLinalgConversionPatterns(patterns);
}
