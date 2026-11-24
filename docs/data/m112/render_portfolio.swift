// Encode an offline, captioned sequence of real captured frames with the system H.264 encoder.
import AVFoundation
import CoreText
import ImageIO

struct Failure: LocalizedError {
    var errorDescription: String?
    init(_ text: String) { errorDescription = text }
}

let width = 1280, height = 720
let outputSize = CGSize(width: width, height: height)

func label(_ context: CGContext, _ text: String, _ size: CGFloat, _ y: CGFloat, _ color: CGColor) {
    let font = CTFontCreateWithName("Helvetica Neue" as CFString, size, nil)
    let attributes = [kCTFontAttributeName: font, kCTForegroundColorAttributeName: color] as CFDictionary
    let rich = CFAttributedStringCreate(nil, text as CFString, attributes)!
    context.textPosition = CGPoint(x: 32, y: y)
    CTLineDraw(CTLineCreateWithAttributedString(rich), context)
}

func frame(_ path: URL, _ title: String, _ detail: String, _ pool: CVPixelBufferPool) throws -> CVPixelBuffer {
    guard let file = CGImageSourceCreateWithURL(path as CFURL, nil),
          let image = CGImageSourceCreateImageAtIndex(file, 0, nil) else { throw Failure("Unreadable capture " + path.path) }
    var allocated: CVPixelBuffer?
    guard CVPixelBufferPoolCreatePixelBuffer(nil, pool, &allocated) == kCVReturnSuccess,
          let buffer = allocated else { throw Failure("Frame allocation failed") }
    CVPixelBufferLockBaseAddress(buffer, [])
    defer { CVPixelBufferUnlockBaseAddress(buffer, []) }
    let layout = CGBitmapInfo.byteOrder32Little.rawValue | CGImageAlphaInfo.premultipliedFirst.rawValue
    guard let context = CGContext(data: CVPixelBufferGetBaseAddress(buffer), width: width, height: height,
                                  bitsPerComponent: 8, bytesPerRow: CVPixelBufferGetBytesPerRow(buffer),
                                  space: CGColorSpaceCreateDeviceRGB(), bitmapInfo: layout)
    else { throw Failure("Bitmap context creation failed") }
    context.setFillColor(red: 0.035, green: 0.055, blue: 0.07, alpha: 1)
    context.fill(CGRect(origin: .zero, size: outputSize))
    let area = CGRect(x: 0, y: 100, width: width, height: height - 100)
    let scale = min(area.width / CGFloat(image.width), area.height / CGFloat(image.height))
    let w = CGFloat(image.width) * scale, h = CGFloat(image.height) * scale
    context.interpolationQuality = .high
    context.draw(image, in: CGRect(x: area.midX - w / 2, y: area.midY - h / 2, width: w, height: h))
    label(context, title, 32, 56, CGColor(gray: 1, alpha: 1))
    label(context, detail, 20, 22, CGColor(gray: 0.85, alpha: 1))
    return buffer
}

func encode(_ root: URL, _ list: URL, _ output: URL) throws {
    let records = try String(contentsOf: list, encoding: .utf8).split(separator: "\n")
    let writer = try AVAssetWriter(outputURL: output, fileType: .mp4)
    let settings: [String: Any] = [AVVideoCodecKey: AVVideoCodecType.h264, AVVideoWidthKey: width, AVVideoHeightKey: height]
    let input = AVAssetWriterInput(mediaType: .video, outputSettings: settings)
    input.expectsMediaDataInRealTime = false
    let attributes: [String: Any] = [kCVPixelBufferPixelFormatTypeKey as String: kCVPixelFormatType_32BGRA,
                                     kCVPixelBufferWidthKey as String: width, kCVPixelBufferHeightKey as String: height]
    let adapter = AVAssetWriterInputPixelBufferAdaptor(assetWriterInput: input, sourcePixelBufferAttributes: attributes)
    guard writer.canAdd(input) else { throw Failure("H.264 input unavailable") }
    writer.add(input)
    guard writer.startWriting() else { throw writer.error ?? Failure("Encoder unavailable") }
    writer.startSession(atSourceTime: .zero)
    guard let pool = adapter.pixelBufferPool else { throw Failure("Missing pixel buffer pool") }
    var seconds: Int64 = 0
    for row in records {
        let cells = row.split(separator: "\t")
        guard cells.count == 4, let duration = Int(cells[0]), duration > 0 else { throw Failure("Invalid storyboard row") }
        let buffer = try frame(root.appendingPathComponent(String(cells[1])), String(cells[2]), String(cells[3]), pool)
        for _ in 0..<duration {
            let deadline = Date().addingTimeInterval(15)
            while !input.isReadyForMoreMediaData {
                if writer.status != .writing || Date() >= deadline { throw writer.error ?? Failure("Encoder stalled") }
                Thread.sleep(forTimeInterval: 0.005)
            }
            guard adapter.append(buffer, withPresentationTime: CMTime(value: seconds, timescale: 1))
            else { throw writer.error ?? Failure("Sample was rejected") }
            seconds += 1
        }
    }
    writer.endSession(atSourceTime: CMTime(value: seconds, timescale: 1))
    input.markAsFinished()
    let done = DispatchSemaphore(value: 0)
    writer.finishWriting { done.signal() }
    guard done.wait(timeout: .now() + 30) == .success, writer.status == .completed
    else { throw writer.error ?? Failure("Writing did not complete") }

    let movie = AVURLAsset(url: output)
    let reader = try AVAssetReader(asset: movie)
    guard let track = movie.tracks(withMediaType: .video).first else { throw Failure("Missing video track") }
    let decoded = AVAssetReaderTrackOutput(track: track, outputSettings: [kCVPixelBufferPixelFormatTypeKey as String: kCVPixelFormatType_32BGRA])
    reader.add(decoded)
    guard reader.startReading() else { throw reader.error ?? Failure("Cannot decode video") }
    var samples = 0
    while let sample = decoded.copyNextSampleBuffer() {
        guard let pixel = CMSampleBufferGetImageBuffer(sample), CVPixelBufferGetWidth(pixel) == width,
              CVPixelBufferGetHeight(pixel) == height else { throw Failure("Decoded size mismatch") }
        samples += 1
    }
    guard reader.status == .completed, samples == seconds, movie.duration.seconds == Double(seconds)
    else { throw Failure("Decoded duration or sample count mismatch") }
    print("captured_images=\(records.count) decoded_samples=\(samples) duration_s=\(seconds) raster=\(width)x\(height) codec=H264")
}

do {
    guard CommandLine.arguments.count == 4 else { throw Failure("usage: render_portfolio ROOT SCENES.tsv NEW.mp4") }
    let urls = CommandLine.arguments.dropFirst().map { URL(fileURLWithPath: $0) }
    try encode(urls[0], urls[1], urls[2])
} catch {
    FileHandle.standardError.write(Data((error.localizedDescription + "\n").utf8))
    exit(1)
}
