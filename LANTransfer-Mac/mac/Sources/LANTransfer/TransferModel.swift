import AppKit
import Foundation

struct Peer: Identifiable, Hashable {
    let ip: String
    let name: String
    var id: String { ip }
}

final class TransferModel: ObservableObject, @unchecked Sendable {
    static let shared = TransferModel()

    @Published var peers: [Peer] = []
    @Published var selectedPeer: String?
    @Published var manualIP = ""
    @Published var queue: [URL] = []
    @Published var progress: Double = 0
    @Published var status = "Hazır"
    @Published var busy = false
    @Published var detail = ""
    @Published var active = false

    let deviceName = Host.current().localizedName ?? ProcessInfo.processInfo.hostName
    let saveDir = FileManager.default.urls(for: .downloadsDirectory, in: .userDomainMask)[0]
        .appendingPathComponent("Transfer")

    private let lock = NSLock()
    private var seen: [String: (name: String, time: Date)] = [:]
    private var ownIPs = Set<UInt32>()
    private var started = false

    private func ui(_ f: @escaping () -> Void) { DispatchQueue.main.async(execute: f) }

    func start() {
        guard !started else { return }
        started = true
        signal(SIGPIPE, SIG_IGN)
        try? FileManager.default.createDirectory(at: saveDir, withIntermediateDirectories: true)
        Thread { self.broadcastLoop() }.start()
        Thread { self.discoveryLoop() }.start()
        Thread { self.serverLoop() }.start()
        Timer.scheduledTimer(withTimeInterval: 1, repeats: true) { [weak self] _ in self?.refreshPeers() }
    }

    // MARK: Kuyruk

    func add(_ urls: [URL]) {
        for u in urls where !queue.contains(u) { queue.append(u) }
    }

    func handleDrop(_ providers: [NSItemProvider]) {
        for p in providers {
            _ = p.loadObject(ofClass: URL.self) { url, _ in
                if let url { self.ui { self.add([url]) } }
            }
        }
    }

    func pickFiles() {
        let panel = NSOpenPanel()
        panel.canChooseFiles = true
        panel.canChooseDirectories = true
        panel.allowsMultipleSelection = true
        if panel.runModal() == .OK { add(panel.urls) }
    }

    // MARK: Keşif

    // Keşif paketi: "LNT2" + tür ('A' = duyuru, 'R' = cevap) + cihaz adı
    private var discFd: Int32 = -1

    private func discPacket(_ kind: UInt8) -> Data {
        var d = Data(Proto.magic)
        d.append(kind)
        d.append(Data(deviceName.utf8))
        return d
    }

    @discardableResult
    private func sendDisc(_ pkt: Data, to ip: UInt32) -> Int32 {
        var a = sockAddr(ip, Proto.discoveryPort)
        let r = pkt.withUnsafeBytes { raw in
            withSockaddr(&a) { sendto(discFd, raw.baseAddress, pkt.count, 0, $0, $1) }
        }
        return r < 0 ? errno : 0
    }

    private func broadcastLoop() {
        while discFd < 0 { usleep(100_000) }
        let pkt = discPacket(UInt8(ascii: "A"))
        var round = 0
        while true {
            let ifs = localInterfaces()
            lock.lock(); ownIPs = Set(ifs.map { $0.ip }); lock.unlock()
            var lastErr: Int32 = 0
            for t in Set([0xFFFF_FFFF] + ifs.map { $0.broadcast }) {
                let e = sendDisc(pkt, to: t)
                if e != 0 { lastErr = e }
            }
            // Modem yayınları engelliyorsa: alt ağdaki her adrese doğrudan gönder (3 turda bir)
            if round % 3 == 0 {
                for i in ifs {
                    let hosts = ~i.mask
                    guard hosts > 1 && hosts <= 1023 else { continue }
                    let net = i.ip & i.mask
                    for h in 1..<hosts where net | h != i.ip {
                        sendDisc(pkt, to: net | h)
                    }
                }
            }
            if lastErr == EHOSTUNREACH || lastErr == EPERM || lastErr == EACCES {
                ui { self.status = "⚠️ Yerel ağ erişimi engelli: Sistem Ayarları → Gizlilik ve Güvenlik → Yerel Ağ → LANTransfer'i açın" }
            }
            round += 1
            sleep(2)
        }
    }

    private func discoveryLoop() {
        let fd = socket(AF_INET, SOCK_DGRAM, 0)
        setOpt(fd, SO_REUSEADDR)
        setOpt(fd, SO_REUSEPORT)
        setOpt(fd, SO_BROADCAST)
        var a = sockAddr(0, Proto.discoveryPort)
        guard withSockaddr(&a, { bind(fd, $0, $1) }) == 0 else {
            ui { self.status = "Keşif portu (\(Proto.discoveryPort)) açılamadı" }
            return
        }
        discFd = fd
        let reply = discPacket(UInt8(ascii: "R"))
        var buf = [UInt8](repeating: 0, count: 1500)
        while true {
            var from = sockaddr_in()
            var len = socklen_t(MemoryLayout<sockaddr_in>.size)
            let n = buf.withUnsafeMutableBytes { raw in
                withUnsafeMutablePointer(to: &from) {
                    $0.withMemoryRebound(to: sockaddr.self, capacity: 1) {
                        recvfrom(fd, raw.baseAddress, raw.count, 0, $0, &len)
                    }
                }
            }
            guard n >= 5, Array(buf[0..<4]) == Proto.magic else { continue }
            let ip = UInt32(bigEndian: from.sin_addr.s_addr)
            let kind = buf[4]
            let name = String(decoding: buf[5..<n], as: UTF8.self)
            lock.lock()
            let mine = ownIPs.contains(ip) || ip >> 24 == 127
            if !mine { seen[ipString(ip)] = (name, Date()) }
            lock.unlock()
            if !mine && kind == UInt8(ascii: "A") { sendDisc(reply, to: ip) }
        }
    }

    private func refreshPeers() {
        lock.lock()
        seen = seen.filter { Date().timeIntervalSince($0.value.time) < 8 }
        let list = seen.map { Peer(ip: $0.key, name: $0.value.name) }.sorted { $0.name < $1.name }
        lock.unlock()
        if list != peers { peers = list }
        // Seçili cihaz yoksa (veya listeden çıktıysa) ilk cihazı seç
        if selectedPeer == nil || !list.contains(where: { $0.ip == selectedPeer }) {
            selectedPeer = list.first?.ip
        }
    }

    // MARK: Alma

    private func serverLoop() {
        let fd = socket(AF_INET, SOCK_STREAM, 0)
        setOpt(fd, SO_REUSEADDR)
        var a = sockAddr(0, Proto.transferPort)
        guard withSockaddr(&a, { bind(fd, $0, $1) }) == 0, listen(fd, 8) == 0 else {
            ui { self.status = "Aktarım portu (\(Proto.transferPort)) açılamadı" }
            return
        }
        while true {
            let c = accept(fd, nil, nil)
            if c < 0 { continue }
            Thread { self.receive(Sock(c)) }.start()
        }
    }

    private func uniqueName(_ name: String) -> String {
        let fm = FileManager.default
        if !fm.fileExists(atPath: saveDir.appendingPathComponent(name).path) { return name }
        let ns = name as NSString
        let ext = ns.pathExtension.isEmpty ? "" : "." + ns.pathExtension
        let stem = ns.deletingPathExtension
        var i = 1
        while true {
            let cand = "\(stem) (\(i))\(ext)"
            if !fm.fileExists(atPath: saveDir.appendingPathComponent(cand).path) { return cand }
            i += 1
        }
    }

    private func receive(_ s: Sock) {
        do {
            guard let m = s.recvExact(4), Array(m) == Proto.magic else { return }
            let sender = try s.readString()
            let count = Int(try s.readUInt(4))
            var files: [(path: String, size: UInt64)] = []
            for _ in 0..<count {
                let p = try s.readString()
                files.append((p, try s.readUInt(8)))
            }
            let total = files.reduce(UInt64(0)) { $0 + $1.size }
            var done: UInt64 = 0
            var topMap: [String: String] = [:]
            var lastUI = Date.distantPast
            var meter = RateMeter()
            let fm = FileManager.default

            for f in files {
                var comps = f.path.split(separator: "/").map(String.init)
                    .filter { !$0.isEmpty && $0 != "." && $0 != ".." }
                if comps.isEmpty { comps = ["dosya"] }
                if topMap[comps[0]] == nil { topMap[comps[0]] = uniqueName(comps[0]) }
                comps[0] = topMap[comps[0]]!
                let dest = comps.reduce(saveDir) { $0.appendingPathComponent($1) }
                try fm.createDirectory(at: dest.deletingLastPathComponent(), withIntermediateDirectories: true)
                guard fm.createFile(atPath: dest.path, contents: nil) else {
                    throw TransferError("Dosya oluşturulamadı: \(dest.lastPathComponent)")
                }
                let fh = try FileHandle(forWritingTo: dest)
                defer { try? fh.close() }
                var rem = f.size
                while rem > 0 {
                    let n = Int(min(UInt64(Proto.chunk), rem))
                    guard let d = s.recvExact(n) else { throw TransferError("Bağlantı kesildi") }
                    fh.write(d)
                    rem -= UInt64(n)
                    done += UInt64(n)
                    if Date().timeIntervalSince(lastUI) > 0.1 {
                        lastUI = Date()
                        let p = total > 0 ? Double(done) / Double(total) : 1
                        let name = comps.last!
                        let txt = meter.text(done: done, total: total)
                        ui { self.active = true; self.progress = p; self.detail = txt; self.status = "Alınıyor ← \(sender): \(name)" }
                    }
                }
            }
            _ = s.sendAll(Data("OK".utf8))
            ui {
                self.progress = 1
                self.active = false
                self.detail = meter.summary(total: total)
                self.status = "\(files.count) dosya alındı ← \(sender)  (\(self.saveDir.path))"
                NSSound(named: "Glass")?.play()
                NSApp.requestUserAttention(.informationalRequest)
            }
        } catch {
            ui { self.active = false; self.detail = ""; self.status = "Alma hatası: \(error)" }
        }
    }

    // MARK: Gönderme

    func send() {
        let manual = manualIP.trimmingCharacters(in: .whitespaces)
        guard let target = manual.isEmpty ? selectedPeer : manual, let ip = parseIP(target) else {
            status = "Önce bir cihaz seçin veya geçerli bir IP girin"
            return
        }
        guard !queue.isEmpty else { status = "Gönderilecek dosya yok"; return }
        let items = queue
        busy = true
        progress = 0
        Thread { self.sendFiles(items, ip) }.start()
    }

    private func collect(_ urls: [URL]) -> [(url: URL, rel: String, size: UInt64)] {
        let fm = FileManager.default
        var out: [(URL, String, UInt64)] = []
        func size(_ path: String) -> UInt64 {
            ((try? fm.attributesOfItem(atPath: path))?[.size] as? NSNumber)?.uint64Value ?? 0
        }
        for u in urls {
            var isDir: ObjCBool = false
            guard fm.fileExists(atPath: u.path, isDirectory: &isDir) else { continue }
            if !isDir.boolValue {
                out.append((u, u.lastPathComponent, size(u.path)))
                continue
            }
            let base = u.lastPathComponent
            let en = fm.enumerator(atPath: u.path)
            while let rel = en?.nextObject() as? String {
                let full = u.appendingPathComponent(rel)
                var d: ObjCBool = false
                guard fm.fileExists(atPath: full.path, isDirectory: &d), !d.boolValue,
                      full.lastPathComponent != ".DS_Store" else { continue }
                out.append((full, base + "/" + rel, size(full.path)))
            }
        }
        return out
    }

    private func sendFiles(_ items: [URL], _ ip: UInt32) {
        do {
            let files = collect(items)
            guard !files.isEmpty else { throw TransferError("Gönderilecek dosya bulunamadı") }
            var hdr = Data(Proto.magic)
            hdr.appendString(deviceName)
            hdr.appendBE(UInt32(files.count))
            for f in files { hdr.appendString(f.rel); hdr.appendBE(f.size) }
            let total = files.reduce(UInt64(0)) { $0 + $1.size }

            ui { self.status = "Bağlanıyor → \(ipString(ip))…" }
            guard let s = connectTCP(ip, Proto.transferPort) else {
                throw TransferError("\(ipString(ip)) adresine bağlanılamadı")
            }
            guard s.sendAll(hdr) else { throw TransferError("Bağlantı kesildi") }

            var done: UInt64 = 0
            var lastUI = Date.distantPast
            var meter = RateMeter()
            for f in files {
                let fh = try FileHandle(forReadingFrom: f.url)
                defer { try? fh.close() }
                var rem = f.size
                while rem > 0 {
                    let d = fh.readData(ofLength: Int(min(UInt64(Proto.chunk), rem)))
                    if d.isEmpty { throw TransferError("Okunamadı: \(f.rel)") }
                    guard s.sendAll(d) else { throw TransferError("Bağlantı kesildi") }
                    rem -= UInt64(d.count)
                    done += UInt64(d.count)
                    if Date().timeIntervalSince(lastUI) > 0.1 {
                        lastUI = Date()
                        let p = total > 0 ? Double(done) / Double(total) : 1
                        let name = f.url.lastPathComponent
                        let txt = meter.text(done: done, total: total)
                        ui { self.active = true; self.progress = p; self.detail = txt; self.status = "Gönderiliyor → \(name)" }
                    }
                }
            }
            guard let ok = s.recvExact(2), ok == Data("OK".utf8) else {
                throw TransferError("Karşı taraf onay vermedi")
            }
            ui {
                self.busy = false
                self.progress = 1
                self.queue.removeAll { items.contains($0) }
                self.active = false
                self.detail = meter.summary(total: total)
                self.status = "\(files.count) dosya gönderildi ✓"
            }
        } catch {
            ui { self.busy = false; self.active = false; self.detail = ""; self.status = "Gönderme hatası: \(error)" }
        }
    }
}
