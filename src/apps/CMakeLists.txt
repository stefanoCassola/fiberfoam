# Helper function to create app targets
function(add_fiberfoam_app APP_NAME)
    add_executable(${APP_NAME} ${APP_NAME}/main.cpp)
    target_link_libraries(${APP_NAME} PRIVATE fiberfoam)
    install(TARGETS ${APP_NAME} RUNTIME DESTINATION bin)
endfunction()

add_fiberfoam_app(fiberFoamMesh)
add_fiberfoam_app(fiberFoamPostProcess)
add_fiberfoam_app(fiberFoamRun)
add_fiberfoam_app(fiberFoamInfo)

if(ONNX_AVAILABLE)
    add_fiberfoam_app(fiberFoamPredict)
    add_fiberfoam_app(fiberFoamConvertModel)
endif()
