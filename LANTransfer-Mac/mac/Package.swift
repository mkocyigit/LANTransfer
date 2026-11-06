// swift-tools-version:5.9
import PackageDescription

let package = Package(
    name: "LANTransfer",
    platforms: [.macOS(.v13)],
    targets: [
        .executableTarget(name: "LANTransfer", path: "Sources/LANTransfer")
    ]
)
