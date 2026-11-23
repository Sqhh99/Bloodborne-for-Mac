// SPDX-License-Identifier: GPL-2.0-or-later
// Trampoline for launching bbgpu-host through LaunchServices (Bloodborne.app).
//
// When bb-probe starts bbgpu-host directly with posix_spawn, RunningBoard treats it as an
// anonymous process (bundleID = nil), so macOS Game Mode's top-right notification banner has no
// app bundle to look up and displays a blank icon. Launching a new instance of Bloodborne.app via
// LaunchServices (`open -n -g Bloodborne.app --args --gpu-host-bootstrap <sock>`) gives the PID a
// proper LaunchServices application identity; this trampoline runs before AppKit initializes,
// receives stdout/stderr and the sanitized environment over the private AF_UNIX socket, and
// replaces itself with bbgpu-host(-macos27) via execve() in the same PID.
import Darwin
import Foundation

enum GpuHostBootstrap {
    private static let maxPayloadBytes = 512 * 1024

    private static func align32(_ n: Int) -> Int {
        (n + 3) & ~3
    }

    static func execIfRequested(arguments: [String]) {
        guard let idx = arguments.firstIndex(of: "--gpu-host-bootstrap"),
              idx + 1 < arguments.count else { return }
        let sockPath = arguments[idx + 1]
        run(socketPath: sockPath)
        _exit(127)
    }

    private static func run(socketPath: String) {
        let fd = socket(AF_UNIX, SOCK_STREAM, 0)
        guard fd >= 0 else { return }

        var addr = sockaddr_un()
        addr.sun_family = sa_family_t(AF_UNIX)
        let pathBytes = Array(socketPath.utf8)
        let maxSunPath = MemoryLayout.size(ofValue: addr.sun_path)
        guard !pathBytes.isEmpty, pathBytes.count < maxSunPath else {
            close(fd)
            return
        }
        withUnsafeMutableBytes(of: &addr.sun_path) { buf in
            for i in 0..<pathBytes.count {
                buf[i] = pathBytes[i]
            }
            buf[pathBytes.count] = 0
        }
        let addrLen = socklen_t(MemoryLayout<sockaddr_un>.offset(of: \.sun_path)! + pathBytes.count + 1)
        let connected = withUnsafePointer(to: &addr) { ptr in
            ptr.withMemoryRebound(to: sockaddr.self, capacity: 1) { sa in
                Darwin.connect(fd, sa, addrLen)
            }
        }
        guard connected == 0 else {
            close(fd)
            return
        }

        // Only accept bootstrap parameters from a process owned by the same user.
        var peerUID: uid_t = 0
        var peerGID: gid_t = 0
        guard getpeereid(fd, &peerUID, &peerGID) == 0, peerUID == geteuid() else {
            close(fd)
            return
        }

        // First message: 4-byte little-endian payload length + SCM_RIGHTS [stdout_fd, stderr_fd].
        var payloadLen: UInt32 = 0
        let cmsgDataOffset = align32(MemoryLayout<cmsghdr>.size)
        var controlBuf = [UInt8](repeating: 0, count: 128)
        let headerRead: Int = withUnsafeMutableBytes(of: &payloadLen) { lenBuf in
            controlBuf.withUnsafeMutableBytes { cBuf in
                var iov = iovec(iov_base: lenBuf.baseAddress, iov_len: lenBuf.count)
                var msg = msghdr()
                msg.msg_iov = withUnsafeMutablePointer(to: &iov) { $0 }
                msg.msg_iovlen = 1
                msg.msg_control = cBuf.baseAddress
                msg.msg_controllen = socklen_t(cBuf.count)
                var n: Int
                repeat {
                    n = recvmsg(fd, &msg, 0)
                } while n < 0 && errno == EINTR
                return n
            }
        }
        guard headerRead == MemoryLayout<UInt32>.size,
              payloadLen > 0, Int(payloadLen) <= maxPayloadBytes else {
            close(fd)
            return
        }

        var passedFds: [Int32] = []
        controlBuf.withUnsafeBytes { cBuf in
            guard cBuf.count >= MemoryLayout<cmsghdr>.size else { return }
            let hdr = cBuf.load(as: cmsghdr.self)
            if hdr.cmsg_level == SOL_SOCKET && hdr.cmsg_type == SCM_RIGHTS {
                let dataBytes = Int(hdr.cmsg_len) - cmsgDataOffset
                let fdCount = dataBytes / MemoryLayout<Int32>.size
                for i in 0..<max(0, min(fdCount, 2)) {
                    let offset = cmsgDataOffset + i * MemoryLayout<Int32>.size
                    if offset + MemoryLayout<Int32>.size <= cBuf.count {
                        let passed = cBuf.load(fromByteOffset: offset, as: Int32.self)
                        passedFds.append(passed)
                    }
                }
            }
        }

        // Read the NUL-delimited payload: host_path\0env0\0env1\0...
        var payload = [UInt8](repeating: 0, count: Int(payloadLen))
        var offset = 0
        while offset < payload.count {
            let n: Int = payload.withUnsafeMutableBytes { buf in
                let base = buf.baseAddress!.advanced(by: offset)
                return Darwin.recv(fd, base, buf.count - offset, 0)
            }
            if n < 0 && errno == EINTR { continue }
            if n <= 0 {
                for pfd in passedFds { close(pfd) }
                close(fd)
                return
            }
            offset += n
        }
        close(fd)

        var strings: [String] = []
        var start = 0
        for i in 0..<payload.count where payload[i] == 0 {
            if let s = String(bytes: payload[start..<i], encoding: .utf8) {
                strings.append(s)
            }
            start = i + 1
        }
        guard let hostPath = strings.first, !hostPath.isEmpty else {
            for pfd in passedFds { close(pfd) }
            return
        }

        // Only execute this bundle's own GPU host binary (resolve symlinks on the directory,
        // keeping checkout apps' symlinks on bbgpu-host* itself intact).
        let macOSDir = Bundle.main.bundleURL.resolvingSymlinksInPath().standardizedFileURL
            .appendingPathComponent("Contents/MacOS").path
        let targetURL = URL(fileURLWithPath: hostPath)
        let targetDir = targetURL.deletingLastPathComponent().resolvingSymlinksInPath().standardizedFileURL.path
        let targetName = targetURL.lastPathComponent
        guard targetDir == macOSDir,
              targetName == "bbgpu-host" || targetName == "bbgpu-host-macos27" else {
            for pfd in passedFds { close(pfd) }
            return
        }

        if passedFds.count >= 2 {
            let outFd = passedFds[0]
            let errFd = passedFds[1]
            // Avoid clobbering errFd if it happens to sit on STDOUT_FILENO.
            var safeErrFd = errFd
            if safeErrFd == STDOUT_FILENO {
                safeErrFd = dup(errFd)
            }
            if outFd >= 0 && outFd != STDOUT_FILENO { dup2(outFd, STDOUT_FILENO) }
            if safeErrFd >= 0 && safeErrFd != STDERR_FILENO { dup2(safeErrFd, STDERR_FILENO) }
            if outFd > STDERR_FILENO { close(outFd) }
            if errFd > STDERR_FILENO && errFd != outFd { close(errFd) }
            if safeErrFd > STDERR_FILENO && safeErrFd != errFd && safeErrFd != outFd { close(safeErrFd) }
        } else {
            for pfd in passedFds { close(pfd) }
        }

        let envStrings = Array(strings.dropFirst())
        let cArgv: [UnsafeMutablePointer<CChar>?] = [strdup(hostPath), nil]
        var cEnvp: [UnsafeMutablePointer<CChar>?] = envStrings.map { strdup($0) }
        cEnvp.append(nil)

        cArgv.withUnsafeBufferPointer { argvBuf in
            cEnvp.withUnsafeBufferPointer { envpBuf in
                _ = hostPath.withCString { cPath in
                    execve(cPath, argvBuf.baseAddress, envpBuf.baseAddress)
                }
            }
        }
    }
}
