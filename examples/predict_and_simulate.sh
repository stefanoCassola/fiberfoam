#!/bin/bash
# Full prediction + simulation pipeline
set -e

INPUT="geometry.dat"
OUTPUT="./case"
VOXEL_SIZE=0.5e-6
VOXEL_RES=320

# Step 1: Predict velocity fields
fiberFoamPredict \
    -input $INPUT \
    -output $OUTPUT \
    -voxelRes $VOXEL_RES \
    -modelRes 80 \
    -modelsDir ./models

# Step 2: Run solver for each direction
for dir in x y z; do
    echo "Running solver for ${dir} direction..."
    simpleFoamMod -case ${OUTPUT}/${dir}_dir
done

# Step 3: Post-process permeability
for dir in x y z; do
    fiberFoamPostProcess \
        -case ${OUTPUT}/${dir}_dir \
        -method both \
        -fibrousRegionOnly
done

echo "Done! Check permeabilityInfo.csv in each case directory."
