#map = affine_map<(d0, d1) -> (d0, d1)>
module {
  func.func @_layer_norm_bwd_dwdb_0123456(%arg0: memref<*xf32>, %arg1: memref<*xf32>, %arg2: memref<*xf32>, %arg3: memref<*xf32>, %arg4: i32, %arg5: i32, %arg6: i32, %arg7: i32, %arg8: i32, %arg9: i32, %arg10: i32, %arg11: i32) {
    %c256_i32 = arith.constant 256 : i32
    %c0_i32 = arith.constant 0 : i32
    %c256 = arith.constant 256 : index
    %cst = arith.constant 0.000000e+00 : f32
    %0 = tensor.empty() : tensor<256x256xf32>
    %1 = linalg.fill ins(%cst : f32) outs(%0 : tensor<256x256xf32>) -> tensor<256x256xf32>
    %2 = arith.muli %arg9, %c256_i32 : i32
    %3 = arith.index_cast %2 : i32 to index
    %4 = arith.index_cast %2 : i32 to index
    %5 = arith.index_cast %2 : i32 to index
    %6 = arith.index_cast %2 : i32 to index
    %7:2 = scf.for %arg12 = %c0_i32 to %arg4 step %c256_i32 iter_args(%arg13 = %1, %arg14 = %1) -> (tensor<256x256xf32>, tensor<256x256xf32>)  : i32 {
      %22 = arith.index_cast %arg12 : i32 to index
      %23 = arith.index_cast %arg5 : i32 to index
      %24 = arith.muli %22, %23 : index
      %25 = arith.addi %24, %6 : index
      %reinterpret_cast_4 = memref.reinterpret_cast %arg0 to offset: [%25], sizes: [256, 256], strides: [%23, 1] : memref<*xf32> to memref<256x256xf32, strided<[?, 1], offset: ?>>
      %26 = arith.index_cast %arg12 : i32 to index
      %27 = arith.addi %26, %c256 : index
      %28 = arith.index_cast %arg4 : i32 to index
      %29 = arith.minsi %27, %28 : index
      %30 = arith.subi %29, %26 : index
      %31 = arith.index_cast %2 : i32 to index
      %32 = arith.addi %31, %c256 : index
      %33 = arith.index_cast %arg5 : i32 to index
      %34 = arith.minsi %32, %33 : index
      %35 = arith.subi %34, %31 : index
      %36 = arith.minsi %30, %c256 : index
      %37 = arith.minsi %35, %c256 : index
      %alloc = memref.alloc() : memref<256x256xf32>
      %38 = arith.cmpi slt, %36, %c256 : index
      %39 = arith.cmpi slt, %37, %c256 : index
      %40 = arith.ori %38, %39 : i1
      scf.if %40 {
        linalg.fill ins(%cst : f32) outs(%alloc : memref<256x256xf32>)
      }
      %subview_5 = memref.subview %reinterpret_cast_4[0, 0] [%36, %37] [1, 1] : memref<256x256xf32, strided<[?, 1], offset: ?>> to memref<?x?xf32, strided<[?, 1], offset: ?>>
      %subview_6 = memref.subview %alloc[0, 0] [%36, %37] [1, 1] : memref<256x256xf32> to memref<?x?xf32, strided<[256, 1]>>
      memref.copy %subview_5, %subview_6 : memref<?x?xf32, strided<[?, 1], offset: ?>> to memref<?x?xf32, strided<[256, 1]>>
      %41 = bufferization.to_tensor %alloc restrict writable : memref<256x256xf32>
      %42 = linalg.generic {indexing_maps = [#map, #map, #map], iterator_types = ["parallel", "parallel"]} ins(%arg13, %41 : tensor<256x256xf32>, tensor<256x256xf32>) outs(%arg13 : tensor<256x256xf32>) {
      ^bb0(%in: f32, %in_11: f32, %out: f32):
        %64 = arith.addf %in, %in_11 : f32
        linalg.yield %64 : f32
      } -> tensor<256x256xf32>
      %43 = arith.index_cast %arg12 : i32 to index
      %44 = arith.index_cast %arg5 : i32 to index
      %45 = arith.muli %43, %44 : index
      %46 = arith.addi %45, %5 : index
      %reinterpret_cast_7 = memref.reinterpret_cast %arg1 to offset: [%46], sizes: [256, 256], strides: [%44, 1] : memref<*xf32> to memref<256x256xf32, strided<[?, 1], offset: ?>>
      %47 = arith.index_cast %arg12 : i32 to index
      %48 = arith.addi %47, %c256 : index
      %49 = arith.index_cast %arg4 : i32 to index
      %50 = arith.minsi %48, %49 : index
      %51 = arith.subi %50, %47 : index
      %52 = arith.index_cast %2 : i32 to index
      %53 = arith.addi %52, %c256 : index
      %54 = arith.index_cast %arg5 : i32 to index
      %55 = arith.minsi %53, %54 : index
      %56 = arith.subi %55, %52 : index
      %57 = arith.minsi %51, %c256 : index
      %58 = arith.minsi %56, %c256 : index
      %alloc_8 = memref.alloc() : memref<256x256xf32>
      %59 = arith.cmpi slt, %57, %c256 : index
      %60 = arith.cmpi slt, %58, %c256 : index
      %61 = arith.ori %59, %60 : i1
      scf.if %61 {
        linalg.fill ins(%cst : f32) outs(%alloc_8 : memref<256x256xf32>)
      }
      %subview_9 = memref.subview %reinterpret_cast_7[0, 0] [%57, %58] [1, 1] : memref<256x256xf32, strided<[?, 1], offset: ?>> to memref<?x?xf32, strided<[?, 1], offset: ?>>
      %subview_10 = memref.subview %alloc_8[0, 0] [%57, %58] [1, 1] : memref<256x256xf32> to memref<?x?xf32, strided<[256, 1]>>
      memref.copy %subview_9, %subview_10 : memref<?x?xf32, strided<[?, 1], offset: ?>> to memref<?x?xf32, strided<[256, 1]>>
      %62 = bufferization.to_tensor %alloc_8 restrict writable : memref<256x256xf32>
      %63 = linalg.generic {indexing_maps = [#map, #map, #map], iterator_types = ["parallel", "parallel"]} ins(%arg14, %62 : tensor<256x256xf32>, tensor<256x256xf32>) outs(%arg14 : tensor<256x256xf32>) {
      ^bb0(%in: f32, %in_11: f32, %out: f32):
        %64 = arith.addf %in, %in_11 : f32
        linalg.yield %64 : f32
      } -> tensor<256x256xf32>
      scf.yield %42, %63 : tensor<256x256xf32>, tensor<256x256xf32>
    }
    %8 = tensor.empty() : tensor<256xf32>
    %9 = linalg.fill ins(%cst : f32) outs(%8 : tensor<256xf32>) -> tensor<256xf32>
    %reduced = linalg.reduce ins(%7#0 : tensor<256x256xf32>) outs(%9 : tensor<256xf32>) dimensions = [0] 
      (%in: f32, %init: f32) {
        %22 = arith.addf %in, %init : f32
        linalg.yield %22 : f32
      }
    %10 = tensor.empty() : tensor<256xf32>
    %11 = linalg.fill ins(%cst : f32) outs(%10 : tensor<256xf32>) -> tensor<256xf32>
    %reduced_0 = linalg.reduce ins(%7#1 : tensor<256x256xf32>) outs(%11 : tensor<256xf32>) dimensions = [0] 
      (%in: f32, %init: f32) {
        %22 = arith.addf %in, %init : f32
        linalg.yield %22 : f32
      }
    %reinterpret_cast = memref.reinterpret_cast %arg2 to offset: [%4], sizes: [256], strides: [1] : memref<*xf32> to memref<256xf32, strided<[1], offset: ?>>
    %12 = arith.index_cast %2 : i32 to index
    %13 = arith.addi %12, %c256 : index
    %14 = arith.index_cast %arg5 : i32 to index
    %15 = arith.minsi %13, %14 : index
    %16 = arith.subi %15, %12 : index
    %extracted_slice = tensor.extract_slice %reduced[0] [%16] [1] : tensor<256xf32> to tensor<?xf32>
    %subview = memref.subview %reinterpret_cast[0] [%16] [1] : memref<256xf32, strided<[1], offset: ?>> to memref<?xf32, strided<[1], offset: ?>>
    bufferization.materialize_in_destination %extracted_slice in writable %subview : (tensor<?xf32>, memref<?xf32, strided<[1], offset: ?>>) -> ()
    %reinterpret_cast_1 = memref.reinterpret_cast %arg3 to offset: [%3], sizes: [256], strides: [1] : memref<*xf32> to memref<256xf32, strided<[1], offset: ?>>
    %17 = arith.index_cast %2 : i32 to index
    %18 = arith.addi %17, %c256 : index
    %19 = arith.index_cast %arg5 : i32 to index
    %20 = arith.minsi %18, %19 : index
    %21 = arith.subi %20, %17 : index
    %extracted_slice_2 = tensor.extract_slice %reduced_0[0] [%21] [1] : tensor<256xf32> to tensor<?xf32>
    %subview_3 = memref.subview %reinterpret_cast_1[0] [%21] [1] : memref<256xf32, strided<[1], offset: ?>> to memref<?xf32, strided<[1], offset: ?>>
    bufferization.materialize_in_destination %extracted_slice_2 in writable %subview_3 : (tensor<?xf32>, memref<?xf32, strided<[1], offset: ?>>) -> ()
    return
  }
}

