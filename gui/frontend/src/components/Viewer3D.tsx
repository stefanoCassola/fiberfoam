import { useRef, useMemo, useState } from 'react'
import { Canvas, useFrame } from '@react-three/fiber'
import { OrbitControls, GizmoHelper, GizmoViewport } from '@react-three/drei'
import * as THREE from 'three'

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------
export interface VoxelData {
  /** 3D boolean/numeric array: voxels[x][y][z]. Truthy = solid. */
  voxels: number[][][]
  dimensions: [number, number, number]
}

export interface MeshData {
  vertices: number[][]
  faces: number[][]
}

interface Viewer3DProps {
  voxelData?: VoxelData | null
  meshData?: MeshData | null
  colorSolid?: string
  colorWireframe?: string
  showWireframe?: boolean
}

// ---------------------------------------------------------------------------
// VoxelInstances -- renders solid voxels via InstancedMesh
// ---------------------------------------------------------------------------
function VoxelInstances({
  voxelData,
  colorSolid,
}: {
  voxelData: VoxelData
  colorSolid: string
}) {
  const meshRef = useRef<THREE.InstancedMesh>(null!)
  const dummy = useMemo(() => new THREE.Object3D(), [])

  const { matrices, count } = useMemo(() => {
    const mats: THREE.Matrix4[] = []
    const [nx, ny, nz] = voxelData.dimensions
    const offsetX = nx / 2
    const offsetY = ny / 2
    const offsetZ = nz / 2

    for (let x = 0; x < nx; x++) {
      for (let y = 0; y < ny; y++) {
        for (let z = 0; z < nz; z++) {
          if (voxelData.voxels[x]?.[y]?.[z]) {
            dummy.position.set(x - offsetX, y - offsetY, z - offsetZ)
            dummy.updateMatrix()
            mats.push(dummy.matrix.clone())
          }
        }
      }
    }
    return { matrices: mats, count: mats.length }
  }, [voxelData, dummy])

  // Apply matrices to instanced mesh
  useMemo(() => {
    if (!meshRef.current) return
    matrices.forEach((mat, i) => {
      meshRef.current.setMatrixAt(i, mat)
    })
    meshRef.current.instanceMatrix.needsUpdate = true
  }, [matrices])

  if (count === 0) return null

  return (
    <instancedMesh ref={meshRef} args={[undefined, undefined, count]}>
      <boxGeometry args={[0.95, 0.95, 0.95]} />
      <meshStandardMaterial color={colorSolid} transparent opacity={0.85} />
    </instancedMesh>
  )
}

// ---------------------------------------------------------------------------
// WireframeMesh -- renders mesh data as wireframe
// ---------------------------------------------------------------------------
function WireframeMesh({
  meshData,
  color,
}: {
  meshData: MeshData
  color: string
}) {
  const geometry = useMemo(() => {
    const geo = new THREE.BufferGeometry()
    const vertices = new Float32Array(meshData.vertices.flat())
    geo.setAttribute('position', new THREE.BufferAttribute(vertices, 3))

    if (meshData.faces.length > 0) {
      const indices: number[] = []
      for (const face of meshData.faces) {
        if (face.length === 3) {
          indices.push(face[0], face[1], face[2])
        } else if (face.length === 4) {
          indices.push(face[0], face[1], face[2])
          indices.push(face[0], face[2], face[3])
        }
      }
      geo.setIndex(indices)
    }
    geo.computeVertexNormals()
    return geo
  }, [meshData])

  return (
    <mesh geometry={geometry}>
      <meshStandardMaterial color={color} wireframe transparent opacity={0.6} />
    </mesh>
  )
}

// ---------------------------------------------------------------------------
// Slow rotation wrapper
// ---------------------------------------------------------------------------
function RotatingGroup({ children, enabled }: { children: React.ReactNode; enabled: boolean }) {
  const ref = useRef<THREE.Group>(null!)
  useFrame((_state, delta) => {
    if (enabled && ref.current) {
      ref.current.rotation.y += delta * 0.15
    }
  })
  return <group ref={ref}>{children}</group>
}

// ---------------------------------------------------------------------------
// Main Viewer3D component
// ---------------------------------------------------------------------------
export default function Viewer3D({
  voxelData,
  meshData,
  colorSolid = '#3b82f6',
  colorWireframe = '#60a5fa',
  showWireframe = false,
}: Viewer3DProps) {
  const [autoRotate, setAutoRotate] = useState(false)

  const hasContent = !!(voxelData || meshData)

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
        camera={{ position: [30, 20, 30], fov: 50, near: 0.1, far: 1000 }}
        gl={{ antialias: true }}
      >
        <color attach="background" args={['#0a0e1a']} />
        <ambientLight intensity={0.4} />
        <directionalLight position={[10, 10, 10]} intensity={0.8} />
        <directionalLight position={[-10, -5, -10]} intensity={0.3} />

        <RotatingGroup enabled={autoRotate}>
          {voxelData && !showWireframe && (
            <VoxelInstances voxelData={voxelData} colorSolid={colorSolid} />
          )}
          {meshData && (
            <WireframeMesh meshData={meshData} color={colorWireframe} />
          )}
        </RotatingGroup>

        <gridHelper args={[50, 50, '#1e293b', '#1e293b']} />
        <OrbitControls makeDefault enableDamping dampingFactor={0.1} />
        <GizmoHelper alignment="bottom-right" margin={[60, 60]}>
          <GizmoViewport labelColor="white" axisHeadScale={0.8} />
        </GizmoHelper>
      </Canvas>
    </div>
  )
}
