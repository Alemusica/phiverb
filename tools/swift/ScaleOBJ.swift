import Foundation
import ModelIO
import MetalKit

/// Scale OBJ positions using Model I/O (Apple Silicon friendly).
///
/// Usage from Swift:
/// ```
/// try scaleOBJ(inputURL: URL(fileURLWithPath: "in.obj"),
///              outputURL: URL(fileURLWithPath: "out.obj"),
///              scale: 0.25)
/// ```
/// - Parameters:
///   - inputURL: Path to the source OBJ.
///   - outputURL: Destination path.
///   - scale: Uniform scale factor applied to positions.
/// - Throws: Any `NSError` raised by Model I/O reading/writing.
public func scaleOBJ(inputURL: URL, outputURL: URL, scale: Float) throws {
    let allocator = MTKMeshBufferAllocator(device: MTLCreateSystemDefaultDevice()!)
    let asset = MDLAsset(url: inputURL, vertexDescriptor: nil, bufferAllocator: allocator)
    asset.loadTextures()

    for case let mesh as MDLMesh in asset {
        guard let positionAttribute = mesh.vertexAttributeData(forAttributeNamed: MDLVertexAttributePosition,
                                                               as: .float3) else { continue }

        let vertexCount = mesh.vertexCount
        let stride = positionAttribute.stride
        var pointer = positionAttribute.dataStart

        for _ in 0..<vertexCount {
            pointer.bindMemory(to: SIMD3<Float>.self, capacity: 1).pointee *= scale
            pointer = pointer.advanced(by: stride)
        }
    }

    try asset.export(to: outputURL)
}
