import Foundation
import Darwin

/// Ortak protokol (Windows sürümüyle aynı). Tüm sayılar big-endian.
/// Keşif (UDP 50000): "LNT2" + tür (A=duyuru, R=cevap) + cihaz adı (UTF-8)
/// Aktarım (TCP 50001): "LNT2" | u16 adUzunluğu | ad | u32 dosyaSayısı |
///   her dosya için: u16 yolUzunluğu | yol ("/" ayraçlı, UTF-8) | u64 boyut
///   ardından dosya içerikleri sırayla; alıcı sonunda "OK" gönderir.
enum Proto {
    static let magic: [UInt8] = Array("LNT2".utf8)
    static let discoveryPort: UInt16 = 50000
    static let transferPort: UInt16 = 50001
    static let chunk = 1 << 20
}

struct TransferError: Error, CustomStringConvertible {
    let description: String
    init(_ d: String) { description = d }
}

struct LocalIf { let ip: UInt32; let broadcast: UInt32; let mask: UInt32 }

func localInterfaces() -> [LocalIf] {
    var res: [LocalIf] = []
    var ifap: UnsafeMutablePointer<ifaddrs>?
    guard getifaddrs(&ifap) == 0 else { return [] }
    defer { freeifaddrs(ifap) }
    var p = ifap
    while let ifa = p {
        p = ifa.pointee.ifa_next
        let flags = Int32(ifa.pointee.ifa_flags)
        guard flags & IFF_UP != 0, flags & IFF_LOOPBACK == 0,
              let addr = ifa.pointee.ifa_addr, addr.pointee.sa_family == sa_family_t(AF_INET),
              let mask = ifa.pointee.ifa_netmask else { continue }
        let ip = addr.withMemoryRebound(to: sockaddr_in.self, capacity: 1) { UInt32(bigEndian: $0.pointee.sin_addr.s_addr) }
        let m = mask.withMemoryRebound(to: sockaddr_in.self, capacity: 1) { UInt32(bigEndian: $0.pointee.sin_addr.s_addr) }
        res.append(LocalIf(ip: ip, broadcast: ip | ~m, mask: m))
    }
    return res
}

func ipString(_ ip: UInt32) -> String {
    "\(ip >> 24 & 255).\(ip >> 16 & 255).\(ip >> 8 & 255).\(ip & 255)"
}

func parseIP(_ s: String) -> UInt32? {
    var a = in_addr()
    guard inet_pton(AF_INET, s, &a) == 1 else { return nil }
    return UInt32(bigEndian: a.s_addr)
}

func sockAddr(_ ip: UInt32, _ port: UInt16) -> sockaddr_in {
    var a = sockaddr_in()
    a.sin_len = UInt8(MemoryLayout<sockaddr_in>.size)
    a.sin_family = sa_family_t(AF_INET)
    a.sin_port = port.bigEndian
    a.sin_addr.s_addr = ip.bigEndian
    return a
}

func withSockaddr<T>(_ a: inout sockaddr_in, _ body: (UnsafePointer<sockaddr>, socklen_t) -> T) -> T {
    withUnsafePointer(to: &a) {
        $0.withMemoryRebound(to: sockaddr.self, capacity: 1) { body($0, socklen_t(MemoryLayout<sockaddr_in>.size)) }
    }
}

func setOpt(_ fd: Int32, _ opt: Int32) {
    var on: Int32 = 1
    setsockopt(fd, SOL_SOCKET, opt, &on, socklen_t(MemoryLayout<Int32>.size))
}

final class Sock {
    let fd: Int32
    init(_ fd: Int32) { self.fd = fd; setOpt(fd, SO_NOSIGPIPE) }
    deinit { close(fd) }

    func sendAll(_ d: Data) -> Bool {
        if d.isEmpty { return true }
        return d.withUnsafeBytes { raw -> Bool in
            var off = 0
            while off < d.count {
                let n = Darwin.send(fd, raw.baseAddress! + off, d.count - off, 0)
                if n < 0 && errno == EINTR { continue }
                if n <= 0 { return false }
                off += n
            }
            return true
        }
    }

    func recvExact(_ n: Int) -> Data? {
        if n == 0 { return Data() }
        var buf = Data(count: n)
        var off = 0
        while off < n {
            let r = buf.withUnsafeMutableBytes { Darwin.recv(fd, $0.baseAddress! + off, n - off, 0) }
            if r < 0 && errno == EINTR { continue }
            if r <= 0 { return nil }
            off += r
        }
        return buf
    }

    func readUInt(_ bytes: Int) throws -> UInt64 {
        guard let d = recvExact(bytes) else { throw TransferError("Bağlantı kesildi") }
        return d.reduce(0) { $0 << 8 | UInt64($1) }
    }

    func readString() throws -> String {
        let n = Int(try readUInt(2))
        guard let d = recvExact(n) else { throw TransferError("Bağlantı kesildi") }
        return String(decoding: d, as: UTF8.self)
    }
}

extension Data {
    mutating func appendBE<T: FixedWidthInteger>(_ v: T) {
        var b = v.bigEndian
        Swift.withUnsafeBytes(of: &b) { append(contentsOf: $0) }
    }
    mutating func appendString(_ s: String) {
        let u = Data(s.utf8.prefix(65535))
        appendBE(UInt16(u.count))
        append(u)
    }
}

func connectTCP(_ ip: UInt32, _ port: UInt16, timeoutMs: Int32 = 5000) -> Sock? {
    let fd = socket(AF_INET, SOCK_STREAM, 0)
    guard fd >= 0 else { return nil }
    let s = Sock(fd)
    let fl = fcntl(fd, F_GETFL)
    _ = fcntl(fd, F_SETFL, fl | O_NONBLOCK)
    var a = sockAddr(ip, port)
    if withSockaddr(&a, { Darwin.connect(fd, $0, $1) }) != 0 {
        guard errno == EINPROGRESS else { return nil }
        var pfd = pollfd(fd: fd, events: Int16(POLLOUT), revents: 0)
        guard poll(&pfd, 1, timeoutMs) == 1 else { return nil }
        var err: Int32 = 0
        var len = socklen_t(MemoryLayout<Int32>.size)
        getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len)
        guard err == 0 else { return nil }
    }
    _ = fcntl(fd, F_SETFL, fl)
    return s
}

func fmtBytes(_ b: Double) -> String {
    let u = ["B", "KB", "MB", "GB", "TB"]
    var v = b, i = 0
    while v >= 1024 && i < u.count - 1 { v /= 1024; i += 1 }
    return i == 0 ? "\(Int(v)) B" : String(format: "%.1f %@", v, u[i])
}

func fmtTime(_ s: Double) -> String {
    guard s.isFinite, s >= 0 else { return "--:--" }
    let t = Int(s.rounded())
    return t >= 3600 ? String(format: "%d:%02d:%02d", t / 3600, t / 60 % 60, t % 60)
                     : String(format: "%d:%02d", t / 60, t % 60)
}

/// Aktarım hızı ve kalan süre hesaplayıcı (yumuşatılmış)
struct RateMeter {
    let start = Date()
    private var lastT = Date()
    private var lastB: UInt64 = 0
    private var rate: Double = 0

    mutating func text(done: UInt64, total: UInt64) -> String {
        let now = Date()
        let dt = now.timeIntervalSince(lastT)
        if dt > 0 {
            let inst = Double(done - lastB) / dt
            rate = rate == 0 ? inst : rate * 0.7 + inst * 0.3
        }
        lastT = now
        lastB = done
        let eta = rate > 0 ? Double(total - done) / rate : .infinity
        return "\(fmtBytes(rate))/s  ·  kalan \(fmtTime(eta))  ·  \(fmtBytes(Double(done))) / \(fmtBytes(Double(total)))"
    }

    func summary(total: UInt64) -> String {
        let el = max(Date().timeIntervalSince(start), 0.001)
        return "\(fmtBytes(Double(total))), \(fmtTime(el)), ort. \(fmtBytes(Double(total) / el))/s"
    }
}
