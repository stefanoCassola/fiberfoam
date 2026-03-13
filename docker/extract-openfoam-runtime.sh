#!/bin/bash
# Extract minimal OpenFOAM runtime: only the shared libraries and binaries
# needed by simpleFoamMod and foamToVTK.
# Source OpenFOAM bashrc first (it returns non-zero for missing ThirdParty
# paths, so we must source it before enabling errexit)
source /usr/lib/openfoam/openfoam2312/etc/bashrc || true

set -eo pipefail

OF=/usr/lib/openfoam/openfoam2312
OUT=/opt/openfoam
mkdir -p "$OUT/lib" "$OUT/bin" "$OUT/etc" "$OUT/lib/dummy" "$OUT/lib/sys-openmpi"

# Collect all OpenFOAM .so deps from foamToVTK and simpleFoam (proxy for simpleFoamMod)
for bin in "$(which foamToVTK)" "$(which simpleFoam)"; do
    ldd "$bin" 2>/dev/null | grep openfoam | awk '{print $3}' >> /tmp/of-libs.txt
done

sort -u /tmp/of-libs.txt | while read -r lib; do
    [ -f "$lib" ] && cp -L "$lib" "$OUT/lib/"
done

# Copy dummy and sys-openmpi Pstream stubs
cp "$OF/platforms/linux64GccDPInt32Opt/lib/dummy/"* "$OUT/lib/dummy/" 2>/dev/null || true
cp "$OF/platforms/linux64GccDPInt32Opt/lib/sys-openmpi/"* "$OUT/lib/sys-openmpi/" 2>/dev/null || true

# Copy foamToVTK binary
cp "$(which foamToVTK)" "$OUT/bin/"

# Copy minimal etc/ for environment sourcing at runtime
cp -r "$OF/etc/"* "$OUT/etc/"

echo "OpenFOAM runtime extraction complete"
du -sh "$OUT/"
ls -la "$OUT/lib/" | head -5
echo "Total libs: $(ls "$OUT/lib/"*.so 2>/dev/null | wc -l)"
