// Read-only OCR of native source art. Recognized words are diagnostic only;
// masks preserve original glyph pixels rather than trusting OCR spelling.
import Foundation
import Vision
import ImageIO

let root = URL(fileURLWithPath: CommandLine.arguments[1])
let data = try Data(contentsOf: root.appendingPathComponent("anchor-jobs.json"))
let jobs = try JSONSerialization.jsonObject(with: data) as! [[String: Any]]
var output: [[String: Any]] = []
for job in jobs {
    let id = job["id"] as! String
    let url = root.appendingPathComponent("active-inputs/\(id).png")
    guard let source = CGImageSourceCreateWithURL(url as CFURL, nil),
          let image = CGImageSourceCreateImageAtIndex(source, 0, nil) else {
        fatalError("Missing native source \(id)")
    }
    let request = VNRecognizeTextRequest()
    request.recognitionLevel = .accurate
    request.usesLanguageCorrection = false
    request.recognitionLanguages = ["en-US", "zh-Hant"]
    request.minimumTextHeight = 0.008
    try VNImageRequestHandler(cgImage: image, options: [:]).perform([request])
    var regions: [[String: Any]] = []
    for observation in request.results ?? [] {
        guard let text = observation.topCandidates(1).first,
              text.confidence >= 0.4, text.string.count >= 2 else { continue }
        let box = observation.boundingBox
        guard box.height < 0.25, box.width < 0.95 else { continue }
        let width = Double(image.width), height = Double(image.height)
        let padX = max(2.0, box.width * width * 0.12)
        let padY = 2.0
        let bounds = [max(0, Int(floor(box.minX * width - padX))),
                      max(0, Int(floor((1 - box.maxY) * height - padY))),
                      min(image.width, Int(ceil(box.maxX * width + padX))),
                      min(image.height, Int(ceil((1 - box.minY) * height + padY)))]
        regions.append(["bbox": bounds, "confidence": text.confidence,
                        "diagnostic_ocr": text.string])
    }
    output.append(["id": id, "views": job["views"]!, "regions": regions])
}
let result = try JSONSerialization.data(withJSONObject: [
    "method": "Native-source Vision OCR regions; preserve source glyphs, never OCR-guessed text",
    "anchors": output], options: [.prettyPrinted, .sortedKeys])
try result.write(to: root.appendingPathComponent("text-regions.json"), options: .atomic)
print("\(output.count) native views inspected for original lettering")
