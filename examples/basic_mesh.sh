#!/bin/bash
# Generate mesh from geometry file for all flow directions
fiberFoamMesh \
    -input geometry.dat \
    -output ./case \
    -voxelSize 0.5e-6 \
    -voxelRes 320 \
    -flowDirection all \
    -connectivity
