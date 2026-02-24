include(GoogleTest)

function(add_fiberfoam_test TEST_NAME SOURCE)
    add_executable(${TEST_NAME} ${SOURCE})
    target_link_libraries(${TEST_NAME} PRIVATE fiberfoam GTest::gtest_main)
    gtest_discover_tests(${TEST_NAME})
endfunction()

# Unit tests
add_fiberfoam_test(test_VoxelArray unit/test_VoxelArray.cpp)
add_fiberfoam_test(test_FiberFreeRegion unit/test_FiberFreeRegion.cpp)
add_fiberfoam_test(test_HexMeshBuilder unit/test_HexMeshBuilder.cpp)
add_fiberfoam_test(test_Connectivity unit/test_Connectivity.cpp)
add_fiberfoam_test(test_FaceGenerator unit/test_FaceGenerator.cpp)
add_fiberfoam_test(test_FiberOrientation unit/test_FiberOrientation.cpp)
add_fiberfoam_test(test_Permeability unit/test_Permeability.cpp)
add_fiberfoam_test(test_Convergence unit/test_Convergence.cpp)
add_fiberfoam_test(test_FoamWriter unit/test_FoamWriter.cpp)
add_fiberfoam_test(test_ModelRegistry unit/test_ModelRegistry.cpp)
add_fiberfoam_test(test_RegionTracker unit/test_RegionTracker.cpp)
add_fiberfoam_test(test_SimulationConfig unit/test_SimulationConfig.cpp)

if(ONNX_AVAILABLE)
    add_fiberfoam_test(test_OnnxPredictor unit/test_OnnxPredictor.cpp)
endif()

# Integration tests
add_fiberfoam_test(test_mesh_pipeline integration/test_mesh_pipeline.cpp)
add_fiberfoam_test(test_predict_pipeline integration/test_predict_pipeline.cpp)
add_fiberfoam_test(test_postprocess_pipeline integration/test_postprocess_pipeline.cpp)
add_fiberfoam_test(test_buffer_pipeline integration/test_buffer_pipeline.cpp)

# Copy fixtures
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/fixtures DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
