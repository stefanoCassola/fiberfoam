import { useRef, useMemo, useState, useEffect } from 'react'
import { Canvas, useThree } from '@react-three/fiber'
import { OrbitControls, GizmoHelper, GizmoViewport } from '@react-three/drei'
import * as THREE from 'three'

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------
export interface VoxelData {
  /** Sparse list of [x, y, z] positions for surface voxels. */
  positions: number[][]
  dimensions: [number, number, number]
}

interface Viewer3DProps {
  voxelData?: VoxelData | null
  colorSolid?: string
}

// ---------------------------------------------------------------------------
// Build a single BufferGeometry from surface voxels.
// Only emits quads for faces adjacent to empty space — far fewer triangles
// than rendering a full box per voxel.
// ---------------------------------------------------------------------------
function buildSurfaceGeometry(data: VoxelData): THREE.BufferGeometry {
  const { positions, dimensions } = data
  const [nx, ny, nz] = dimensions
  const ox = nx / 2, oy = ny / 2, oz = nz / 2

  // Build a Set for O(1) occupancy lookup
  const key = (x: number, y: number, z: number) => `${x},${y},${z}`
  const occupied = new Set<string>()
  for (const p of positions) occupied.add(key(p[0], p[1], p[2]))

  // 6 face directions: [dx,dy,dz] and the 4 corner offsets for each quad
  const faces: { dir: number[]; corners: number[][] }[] = [
    { dir: [1, 0, 0], corners: [[1,0,0],[1,1,0],[1,1,1],[1,0,1]] },  // +x
    { dir: [-1,0,0], corners: [[0,0,1],[0,1,1],[0,1,0],[0,0,0]] },   // -x
    { dir: [0, 1, 0], corners: [[0,1,1],[1,1,1],[1,1,0],[0,1,0]] },  // +y
    { dir: [0,-1,0], corners: [[0,0,0],[1,0,0],[1,0,1],[0,0,1]] },   // -y
    { dir: [0, 0, 1], corners: [[0,0,1],[1,0,1],[1,1,1],[0,1,1]] },  // +z
    { dir: [0, 0,-1], corners: [[1,0,0],[0,0,0],[0,1,0],[1,1,0]] },  // -z
  ]

  const verts: number[] = []
  const normals: number[] = []
  const indices: number[] = []

  for (const p of positions) {
    const [x, y, z] = p
    for (const { dir, corners } of faces) {
      const nx2 = x + dir[0], ny2 = y + dir[1], nz2 = z + dir[2]
      if (occupied.has(key(nx2, ny2, nz2))) continue // neighbor exists, skip face

      const base = verts.length / 3
      for (const c of corners) {
        verts.push(x + c[0] - ox, y + c[1] - oy, z + c[2] - oz)
        normals.push(dir[0], dir[1], dir[2])
      }
      indices.push(base, base + 1, base + 2, base, base + 2, base + 3)
    }
  }

  const geo = new THREE.BufferGeometry()
  geo.setAttribute('position', new THREE.Float32BufferAttribute(verts, 3))
  geo.setAttribute('normal', new THREE.Float32BufferAttribute(normals, 3))
  geo.setIndex(indices)
  return geo
}

// ---------------------------------------------------------------------------
// Auto-fit camera to geometry bounds
// ---------------------------------------------------------------------------
function CameraFit({ dimensions }: { dimensions: [number, number, number] }) {
  const { camera } = useThree()
  const fitted = useRef(false)
  const prevDims = useRef('')

  useEffect(() => {
    const dimKey = dimensions.join(',')
    if (fitted.current && dimKey === prevDims.current) return
    prevDims.current = dimKey

    const maxDim = Math.max(...dimensions)
    const dist = maxDim * 1.4
    camera.position.set(dist * 0.7, dist * 0.5, dist * 0.7)
    camera.lookAt(0, 0, 0)
    camera.updateProjectionMatrix()
    fitted.current = true
  }, [dimensions, camera])

  return null
}

// ---------------------------------------------------------------------------
// VoxelSurface — single draw-call mesh
// ---------------------------------------------------------------------------
function VoxelSurface({ voxelData, color }: { voxelData: VoxelData; color: string }) {
  const geometry = useMemo(() => buildSurfaceGeometry(voxelData), [voxelData])

  useEffect(() => {
    return () => { geometry.dispose() }
  }, [geometry])

  return (
    <mesh geometry={geometry}>
      <meshStandardMaterial color={color} />
    </mesh>
  )
}

// ---------------------------------------------------------------------------
// Main Viewer3D component
// ---------------------------------------------------------------------------
export default function Viewer3D({
  voxelData,
  colorSolid = '#3b82f6',
}: Viewer3DProps) {
  const [autoRotate, setAutoRotate] = useState(true)
  const hasContent = !!voxelData

  return (
    <div className="relative w-full h-full min-h-[400px] rounded-xl overflow-hidden border border-gray-700 bg-gray-950">
      {/* Toolbar */}
      <div className="absolute top-3 right-3 z-10 flex gap-2">
        <button
          onClick={() => setAutoRotate((v) => !v)}
          className={`px-3 py-1.5 text-xs rounded-md font-medium transition-colors ${
            autoRotate
              ? 'bg-primary-600 text-white'
              : 'bg-gray-800 text-gray-400 hover:text-gray-200'
          }`}
        >
          Auto-Rotate
        </button>
      </div>

      {!hasContent && (
        <div className="absolute inset-0 flex items-center justify-center text-gray-600">
          <div className="text-center">
            <svg className="w-16 h-16 mx-auto mb-3 opacity-30" fill="none" viewBox="0 0 24 24" stroke="currentColor">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1} d="M21 7.5l-9-5.25L3 7.5m18 0l-9 5.25m9-5.25v9l-9 5.25M3 7.5l9 5.25M3 7.5v9l9 5.25m0-9v9" />
            </svg>
            <p className="text-sm">No geometry loaded</p>
          </div>
        </div>
      )}

      <Canvas
        camera={{ fov: 45, near: 0.1, far: 5000 }}
        gl={{ antialias: true }}
      >
        <color attach="background" args={['#0a0e1a']} />
        <ambientLight intensity={0.5} />
        <directionalLight position={[10, 10, 10]} intensity={0.8} />
        <directionalLight position={[-10, -5, -10]} intensity={0.3} />

        {voxelData && (
          <>
            <CameraFit dimensions={voxelData.dimensions} />
            <VoxelSurface voxelData={voxelData} color={colorSolid} />
          </>
        )}

        <OrbitControls
          makeDefault
          enableDamping
          dampingFactor={0.15}
          autoRotate={autoRotate}
          autoRotateSpeed={1.5}
          minDistance={1}
          maxDistance={3000}
        />
        <GizmoHelper alignment="bottom-right" margin={[60, 60]}>
          <GizmoViewport labelColor="white" axisHeadScale={0.8} />
        </GizmoHelper>
      </Canvas>
    </div>
  )
}
