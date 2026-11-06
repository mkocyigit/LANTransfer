import AppKit
import ServiceManagement
import SwiftUI
import UniformTypeIdentifiers

// MARK: - Uygulama

final class AppDelegate: NSObject, NSApplicationDelegate {
    func applicationDidFinishLaunching(_ n: Notification) {
        NSApp.setActivationPolicy(.regular)
        TransferModel.shared.start()
        LoginItem.enableOnFirstRun()
    }
    // Pencere kapansa da menü çubuğunda çalışmaya devam et
    func applicationShouldTerminateAfterLastWindowClosed(_ app: NSApplication) -> Bool { false }
}

enum LoginItem {
    static var isEnabled: Bool { SMAppService.mainApp.status == .enabled }

    static func set(_ on: Bool) {
        do {
            if on { try SMAppService.mainApp.register() } else { try SMAppService.mainApp.unregister() }
        } catch {
            TransferModel.shared.status = "Başlangıç ayarı değiştirilemedi: \(error.localizedDescription)"
        }
    }

    static func enableOnFirstRun() {
        let key = "loginItemConfigured"
        guard !UserDefaults.standard.bool(forKey: key) else { return }
        UserDefaults.standard.set(true, forKey: key)
        set(true)
    }
}

@main
struct LANTransferApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) var delegate

    var body: some Scene {
        Window("LAN Transfer", id: "main") {
            ContentView(m: TransferModel.shared)
        }
        .windowStyle(.hiddenTitleBar)
        .defaultSize(width: 760, height: 540)

        MenuBarExtra {
            TrayMenu(m: TransferModel.shared)
        } label: {
            TrayLabel(m: TransferModel.shared)
        }
    }
}

// MARK: - Menü çubuğu

struct TrayLabel: View {
    @ObservedObject var m: TransferModel
    var body: some View {
        if m.active {
            Image(systemName: "arrow.left.arrow.right.circle.fill")
            Text("\(Int(m.progress * 100))%")
        } else {
            Image(systemName: "arrow.left.arrow.right")
        }
    }
}

struct TrayMenu: View {
    @ObservedObject var m: TransferModel
    @Environment(\.openWindow) private var openWindow
    @State private var login = LoginItem.isEnabled

    var body: some View {
        Text(m.status)
        if !m.detail.isEmpty { Text(m.detail) }
        Divider()
        Button("LAN Transfer'i Göster") {
            openWindow(id: "main")
            NSApp.activate(ignoringOtherApps: true)
        }
        Button("Gelen Klasörü Aç") { NSWorkspace.shared.open(m.saveDir) }
        Divider()
        Toggle("Başlangıçta Çalıştır", isOn: Binding(
            get: { LoginItem.isEnabled },
            set: { LoginItem.set($0); login = LoginItem.isEnabled }))
        Divider()
        Button("Çık") { NSApp.terminate(nil) }.keyboardShortcut("q")
    }
}

// MARK: - Ana pencere

struct ContentView: View {
    @ObservedObject var m: TransferModel
    @State private var dropTargeted = false
    @State private var login = LoginItem.isEnabled

    private var myIP: String {
        localInterfaces().map { ipString($0.ip) }.joined(separator: ", ")
    }

    private var targetName: String? {
        let manual = m.manualIP.trimmingCharacters(in: .whitespaces)
        if !manual.isEmpty { return manual }
        return m.peers.first { $0.ip == m.selectedPeer }?.name
    }

    var body: some View {
        VStack(spacing: 0) {
            header
            Divider()
            HStack(spacing: 0) {
                devicePane
                    .frame(width: 240)
                    .background(Color(nsColor: .underPageBackgroundColor).opacity(0.5))
                Divider()
                dropPane
            }
            Divider()
            footer
        }
        .frame(minWidth: 700, minHeight: 500)
        .onDrop(of: [.fileURL], isTargeted: $dropTargeted) { providers in
            m.handleDrop(providers)
            return true
        }
    }

    // Üst bilgi çubuğu
    private var header: some View {
        HStack(spacing: 12) {
            Image(nsImage: NSApp.applicationIconImage)
                .resizable().frame(width: 40, height: 40)
            VStack(alignment: .leading, spacing: 2) {
                Text("LAN Transfer").font(.title3.weight(.semibold))
                Text("\(m.deviceName) · \(myIP)")
                    .font(.caption).foregroundStyle(.secondary)
            }
            Spacer()
            Toggle("Başlangıçta çalıştır", isOn: Binding(
                get: { login },
                set: { LoginItem.set($0); login = LoginItem.isEnabled }))
                .toggleStyle(.switch).controlSize(.small)
            Button {
                NSWorkspace.shared.open(m.saveDir)
            } label: {
                Label("Gelenler", systemImage: "folder")
            }
        }
        .padding(.horizontal, 20)
        .padding(.top, 28)
        .padding(.bottom, 14)
    }

    // Sol: cihazlar
    private var devicePane: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("CİHAZLAR")
                .font(.caption.weight(.semibold)).foregroundStyle(.secondary)
                .padding(.horizontal, 4)
            ScrollView {
                VStack(spacing: 6) {
                    ForEach(m.peers) { p in
                        DeviceRow(peer: p, selected: m.selectedPeer == p.ip && m.manualIP.isEmpty)
                            .onTapGesture { m.selectedPeer = p.ip; m.manualIP = "" }
                    }
                }
            }
            .overlay {
                if m.peers.isEmpty {
                    VStack(spacing: 10) {
                        ProgressView().controlSize(.small)
                        Text("Ağda cihaz aranıyor…")
                            .font(.callout).foregroundStyle(.secondary)
                    }
                }
            }
            HStack(spacing: 6) {
                Image(systemName: "network").foregroundStyle(.secondary)
                TextField("veya IP adresi", text: $m.manualIP)
                    .textFieldStyle(.plain)
            }
            .padding(8)
            .background(RoundedRectangle(cornerRadius: 8).fill(.background))
            .overlay(RoundedRectangle(cornerRadius: 8).stroke(.quaternary))
        }
        .padding(14)
    }

    // Sağ: bırakma alanı ve kuyruk
    private var dropPane: some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack {
                Text("GÖNDERİLECEKLER")
                    .font(.caption.weight(.semibold)).foregroundStyle(.secondary)
                if !m.queue.isEmpty {
                    Text("\(m.queue.count)")
                        .font(.caption2.weight(.bold))
                        .padding(.horizontal, 6).padding(.vertical, 1)
                        .background(Capsule().fill(.quaternary))
                }
                Spacer()
                Button("Ekle…", systemImage: "plus") { m.pickFiles() }
                    .buttonStyle(.borderless)
                if !m.queue.isEmpty {
                    Button("Temizle", systemImage: "trash") { m.queue.removeAll() }
                        .buttonStyle(.borderless).disabled(m.busy)
                }
            }
            ZStack {
                RoundedRectangle(cornerRadius: 14)
                    .strokeBorder(style: StrokeStyle(lineWidth: 2, dash: m.queue.isEmpty ? [7, 5] : []))
                    .foregroundStyle(dropTargeted ? Color.accentColor : Color.secondary.opacity(0.3))
                    .background(RoundedRectangle(cornerRadius: 14)
                        .fill(dropTargeted ? Color.accentColor.opacity(0.08) : .clear))

                if m.queue.isEmpty {
                    VStack(spacing: 12) {
                        Image(systemName: "arrow.down.doc")
                            .font(.system(size: 44, weight: .light))
                            .foregroundStyle(dropTargeted ? Color.accentColor : .secondary)
                        Text("Dosya ve klasörleri buraya bırakın")
                            .font(.headline)
                        Text("veya Ekle… ile seçin")
                            .font(.callout).foregroundStyle(.secondary)
                    }
                } else {
                    ScrollView {
                        VStack(spacing: 2) {
                            ForEach(m.queue, id: \.self) { u in
                                QueueRow(url: u) { m.queue.removeAll { $0 == u } }
                                    .disabled(m.busy)
                            }
                        }
                        .padding(8)
                    }
                }
            }
        }
        .padding(14)
    }

    // Alt: ilerleme ve gönder
    private var footer: some View {
        HStack(spacing: 16) {
            VStack(alignment: .leading, spacing: 6) {
                Text(m.status)
                    .font(.callout.weight(.medium))
                    .lineLimit(1).truncationMode(.middle)
                ProgressView(value: m.progress)
                    .progressViewStyle(.linear)
                    .tint(m.active ? .accentColor : .secondary)
                Text(m.detail.isEmpty ? " " : m.detail)
                    .font(.caption.monospacedDigit()).foregroundStyle(.secondary)
                    .lineLimit(1)
            }
            Button { m.send() } label: {
                HStack(spacing: 8) {
                    if m.busy { ProgressView().controlSize(.small) }
                    else { Image(systemName: "paperplane.fill") }
                    Text(targetName.map { "Gönder → \($0)" } ?? "Gönder")
                        .lineLimit(1)
                }
                .frame(minWidth: 160)
                .padding(.vertical, 4)
            }
            .buttonStyle(.borderedProminent)
            .controlSize(.large)
            .keyboardShortcut(.defaultAction)
            .disabled(m.busy || m.queue.isEmpty || targetName == nil)
        }
        .padding(.horizontal, 20)
        .padding(.vertical, 14)
    }
}

struct DeviceRow: View {
    let peer: Peer
    let selected: Bool

    var body: some View {
        HStack(spacing: 10) {
            Image(systemName: "desktopcomputer")
                .font(.title3)
                .frame(width: 34, height: 34)
                .background(Circle().fill(selected ? Color.white.opacity(0.25) : Color.accentColor.opacity(0.12)))
                .foregroundStyle(selected ? Color.white : Color.accentColor)
            VStack(alignment: .leading, spacing: 1) {
                Text(peer.name).font(.body.weight(.medium)).lineLimit(1)
                Text(peer.ip).font(.caption.monospacedDigit())
                    .foregroundStyle(selected ? Color.white.opacity(0.8) : .secondary)
            }
            Spacer()
            Circle().fill(.green).frame(width: 7, height: 7)
        }
        .padding(8)
        .foregroundStyle(selected ? Color.white : .primary)
        .background(RoundedRectangle(cornerRadius: 10).fill(selected ? Color.accentColor : .clear))
        .contentShape(Rectangle())
    }
}

struct QueueRow: View {
    let url: URL
    let remove: () -> Void
    @State private var hover = false

    private var info: String {
        let v = try? url.resourceValues(forKeys: [.isDirectoryKey, .fileSizeKey])
        if v?.isDirectory == true { return "Klasör" }
        return fmtBytes(Double(v?.fileSize ?? 0))
    }

    var body: some View {
        HStack(spacing: 10) {
            Image(nsImage: NSWorkspace.shared.icon(forFile: url.path))
                .resizable().frame(width: 28, height: 28)
            VStack(alignment: .leading, spacing: 1) {
                Text(url.lastPathComponent).lineLimit(1).truncationMode(.middle)
                Text(info).font(.caption).foregroundStyle(.secondary)
            }
            Spacer()
            Button(action: remove) {
                Image(systemName: "xmark.circle.fill").foregroundStyle(.secondary)
            }
            .buttonStyle(.plain)
            .opacity(hover ? 1 : 0)
        }
        .padding(.horizontal, 8).padding(.vertical, 5)
        .background(RoundedRectangle(cornerRadius: 8).fill(hover ? Color.primary.opacity(0.05) : .clear))
        .onHover { hover = $0 }
    }
}
