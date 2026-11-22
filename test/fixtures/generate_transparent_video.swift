// generates transparent_video.mov: one second of a red rectangle on transparent pixels.
// run on macOS with a new output path as the first argument; no external media is used.
import AVFoundation
import CoreVideo
import Foundation

let url = URL(fileURLWithPath: CommandLine.arguments[1])
let writer = try AVAssetWriter(outputURL: url, fileType: .mov)
let width = 64, height = 48
let input = AVAssetWriterInput(mediaType: .video, outputSettings: [
    AVVideoCodecKey: AVVideoCodecType.proRes4444,
    AVVideoWidthKey: width, AVVideoHeightKey: height])
let adaptor = AVAssetWriterInputPixelBufferAdaptor(assetWriterInput: input,
    sourcePixelBufferAttributes: [kCVPixelBufferPixelFormatTypeKey as String: kCVPixelFormatType_32BGRA,
        kCVPixelBufferWidthKey as String: width, kCVPixelBufferHeightKey as String: height])
writer.add(input)
guard writer.startWriting() else { fatalError(String(describing: writer.error)) }
writer.startSession(atSourceTime: .zero)
for index in 0..<30 {
    var buffer: CVPixelBuffer?
    guard CVPixelBufferPoolCreatePixelBuffer(nil, adaptor.pixelBufferPool!, &buffer) == kCVReturnSuccess else { fatalError("pixel allocation") }
    CVPixelBufferLockBaseAddress(buffer!, [])
    let bytes = CVPixelBufferGetBaseAddress(buffer!)!.assumingMemoryBound(to: UInt8.self)
    let stride = CVPixelBufferGetBytesPerRow(buffer!)
    memset(bytes, 0, stride * height)
    for y in 16..<32 { for x in 16..<48 {
        bytes[y * stride + x * 4 + 2] = 255
        bytes[y * stride + x * 4 + 3] = 255
    }}
    CVPixelBufferUnlockBaseAddress(buffer!, [])
    while !input.isReadyForMoreMediaData { Thread.sleep(forTimeInterval: 0.001) }
    guard adaptor.append(buffer!, withPresentationTime: CMTime(value: Int64(index), timescale: 30)) else { fatalError(String(describing: writer.error)) }
}
input.markAsFinished()
let complete = DispatchSemaphore(value: 0)
writer.finishWriting { complete.signal() }
guard complete.wait(timeout: .now() + 20) == .success && writer.status == .completed else { fatalError(String(describing: writer.error)) }
