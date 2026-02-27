set(FIBERFOAM_SOURCES
    common/Logger.cpp
    config/SimulationConfig.cpp
    geometry/VoxelArray.cpp
    geometry/FiberFreeRegion.cpp
    geometry/RegionTracker.cpp
    mesh/HexMeshBuilder.cpp
    mesh/Connectivity.cpp
    mesh/FaceGenerator.cpp
    analysis/FiberOrientation.cpp
    analysis/VelocityReconstruction.cpp
    io/FoamWriter.cpp
    io/FoamReader.cpp
    io/ArrayIO.cpp
    io/CsvWriter.cpp
    postprocessing/Permeability.cpp
    postprocessing/Convergence.cpp
)

if(ONNX_AVAILABLE)
    list(APPEND FIBERFOAM_SOURCES
        ml/ModelRegistry.cpp
        ml/OnnxPredictor.cpp
        ml/Scaling.cpp
    )
endif()

add_library(fiberfoam STATIC ${FIBERFOAM_SOURCES})

target_include_directories(fiberfoam PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/generated
    ${FFTW3_INCLUDE_DIRS}
)

target_link_libraries(fiberfoam PUBLIC
    Eigen3::Eigen
    ${FFTW3_LIBRARIES}
    yaml-cpp
    nlohmann_json::nlohmann_json
)

if(ONNX_AVAILABLE)
    target_compile_definitions(fiberfoam PUBLIC FIBERFOAM_HAS_ONNX)
    if(onnxruntime_FOUND)
        target_link_libraries(fiberfoam PUBLIC onnxruntime::onnxruntime)
    else()
        target_include_directories(fiberfoam PUBLIC ${ONNXRUNTIME_INCLUDE_DIRS})
        target_link_libraries(fiberfoam PUBLIC ${ONNXRUNTIME_LIBRARIES})
    endif()
endif()
