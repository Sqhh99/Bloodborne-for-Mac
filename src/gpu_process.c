// SPDX-FileCopyrightText: Copyright 2026 Mark Hall
/* Native arm64 GPU process, runtime side (docs/DESIGN_NATIVE_GPU_PROCESS.md, M2 and M3).
 * The only renderer on macOS: spawns out/bbgpu-host (arm64 slice), hands it the guest memory
 * entry and a shared control block (host/bbgpu_protocol.h, host/bbgpu_shared.h) and mirrors
 * every guest view into it at the same address, through the control block's submit ring. Then
 * probe.c starts the renderer there (bbgpu_init_remote) and the GPU work follows on the same
 * ring. If the host cannot start, or is lost at any point, the game stops with a "Crash:" line
 * (the launcher's crash report shows it).
 *
 *   BB_GPU_HOST=path             host executable (default: bbgpu-host next to bb-probe)
 *   BB_GPU_PROCESS_VERIFY_MS=n   period of the mirror verification (default 0 = off; e.g. 5000)
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "gpu_process.h"
#include "runtime.h"
#ifdef __APPLE__
#include "gpu/bbgpu.h"
#include "host/bbgpu_shared.h"
#include <mach/mach_vm.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <libgen.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <mach/mach.h>
#include <mach/notify.h>
#include <poll.h>
#include <pthread.h>
#include <servers/bootstrap.h>
#include <signal.h>
#include <spawn.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char **environ;

#define HANDSHAKE_TIMEOUT_MS 10000
#define LAUNCH_CONNECT_TIMEOUT_MS 5000
#define SEND_TIMEOUT_MS 2000
#define FLUSH_MS 20 /* monitor tick */
#define SAMPLE_BYTES (UINT64_C(256) * 1024)

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static int alive;                  /* mirroring active (under mutex) */
static pid_t host_pid;
static int host_is_child;          /* 1: direct posix_spawn child (waitpid works); 0: LaunchServices */
static mach_port_t host_port = MACH_PORT_NULL, ctl_port = MACH_PORT_NULL;
static BbGpuShared *shared;        /* the control block (bbgpu_shared.h) */
static mach_port_t shared_entry = MACH_PORT_NULL;
static int rendering;              /* the renderer runs in the host: losing it is fatal */
static uint64_t events_sent, verify_seq;
static struct { uint64_t seq; RuntimeMirrorSnapshot snapshot; } pending[4];

typedef union {
    mach_msg_header_t h;
    BbGpuHello hello;
    BbGpuReady ready;
    BbGpuVerifyReply reply;
    mach_dead_name_notification_t dead;
    uint8_t space[sizeof(BbGpuVerifyReply) + 256];
} Inbox;
typedef struct { Inbox m; mach_msg_max_trailer_t trailer; } InboxBuffer;

/* The game cannot run without the GPU process (there is no single-process mode any more). */
static void start_failed(const char *why) {
    printf("Crash: the GPU process could not start (%s). The lines above say why.\n", why);
    fflush(stdout);
    _exit(GPU_PROCESS_EXIT_START);
}

static void lost_locked(const char *why) {
    if (!alive) return;
    alive = 0;
    if (rendering) {
        /* The host exits 0 when its window is closed: the player quit. */
        const int quit = strstr(why, "exited with status 0") != NULL;
        if (quit) printf("GPU process: game window closed; exiting\n");
        else printf("GPU process: host lost (%s) while rendering; exiting\n", why);
        if (!quit) printf("Crash: the GPU process stopped unexpectedly (%s)\n", why);
        fflush(stdout);
        _exit(quit ? 0 : 1);
    }
    printf("GPU process: host lost (%s) before rendering\n", why);
    start_failed(why);
}

/* runtime_memory.c, region table write lock held: pushed in operation order, ahead of any GPU
 * work issued after it (one ring, one producer lock in the GPU library). */
static void mirror_event(int op, uintptr_t address, uint64_t size, uint64_t phys, int prot) {
    pthread_mutex_lock(&mutex);
    const int live = alive;
    if (live) ++events_sent;
    pthread_mutex_unlock(&mutex);
    if (!live) return;
    const BbGpuViewRecord record = {{BBGPU_REC_VIEW, sizeof record}, (uint32_t)op, (uint32_t)prot, address, size, phys};
    bbgpu_split_push(&record);
}

/* runtime_memory.c, region table read lock held: nothing changes until this returns, so the
 * request lands in the event stream exactly after the events that built the snapshot. */
static void emit_verify(const RuntimeMirrorSnapshot *s) {
    BbGpuVerifyRecord v;
    memset(&v, 0, sizeof v);
    pthread_mutex_lock(&mutex);
    const int live = alive;
    if (live) {
        v.r = (BbGpuRecord){BBGPU_REC_VERIFY, sizeof v};
        v.seq = ++verify_seq;
        v.views = s->views; v.bytes = s->bytes; v.events = events_sent; v.count = s->count;
        for (unsigned i = 0; i < s->count && i < 8; ++i) {
            v.samples[i].address = s->samples[i].address;
            v.samples[i].size = s->samples[i].size;
            v.samples[i].checksum = s->samples[i].checksum;
        }
        pending[v.seq % 4].seq = v.seq;
        pending[v.seq % 4].snapshot = *s;
    }
    pthread_mutex_unlock(&mutex);
    if (live) bbgpu_split_push(&v);
}

static void handle_reply(const BbGpuVerifyReply *r) {
    RuntimeMirrorSnapshot s;
    uint64_t sent;
    pthread_mutex_lock(&mutex);
    int known = pending[r->seq % 4].seq == r->seq;
    if (known) s = pending[r->seq % 4].snapshot;
    sent = events_sent;
    pthread_mutex_unlock(&mutex);
    if (!known || r->count != s.count) {
        printf("GPU process: verify #%" PRIu64 ": unexpected reply\n", r->seq);
        return;
    }
    /* The game keeps writing: a sample counts as a match if the host's checksum equals the
     * runtime's at the snapshot or right now; one that changed in between and matches neither
     * is inconclusive, not a mismatch. */
    unsigned match = 0, changed = 0, mismatch = 0, data = 0;
    uint64_t sampled = 0;
    static const uint64_t zero_page[BBGPU_GUEST_PAGE / 8];
    for (unsigned i = 0; i < s.count; ++i) {
        const RuntimeMirrorSample *x = &s.samples[i];
        uint64_t zero = BBGPU_CHECKSUM_SEED; /* checksum of all-zero bytes: such a sample proves less */
        for (uint64_t at = 0; at < x->size; at += BBGPU_GUEST_PAGE) zero = bbgpu_checksum(zero, zero_page, BBGPU_GUEST_PAGE);
        data += x->checksum != zero;
        uint64_t now = 0;
        int have_now = runtime_memory_mirror_checksum(x->address, x->size, &now);
        sampled += x->size;
        if (r->mapped[i] && (r->checksums[i] == x->checksum || (have_now && r->checksums[i] == now))) ++match;
        else if (r->mapped[i] && (!have_now || now != x->checksum)) ++changed;
        else {
            ++mismatch;
            printf("GPU process: verify #%" PRIu64 ": sample 0x%" PRIx64 "+0x%" PRIx64 " %s (runtime %016" PRIx64
                   ", host %016" PRIx64 ")\n", r->seq, x->address, x->size,
                   r->mapped[i] ? "differs" : "not mirrored", x->checksum, r->checksums[i]);
        }
    }
    const int tables = r->views == s.views && r->bytes == s.bytes;
    if (!tables)
        printf("GPU process: verify #%" PRIu64 ": view table differs: runtime %" PRIu64 " views/%" PRIu64
               " bytes, host %" PRIu64 "/%" PRIu64 "\n", r->seq, s.views, s.bytes, r->views, r->bytes);
    printf("GPU process: %" PRIu64 " views mirrored, %s (verify #%" PRIu64 ": %u/%u samples match (%u non-zero), %u changed in flight, "
           "%" PRIu64 " KiB compared; %.2f GiB mapped; %" PRIu64 " events applied, %" PRIu64 " host failures; "
           "%" PRIu64 " events sent)\n",
           r->views, !mismatch && tables && r->failures == 0 ? "checksum match" : "MISMATCH", r->seq, match, s.count, data,
           changed, sampled >> 10, (double)r->bytes / (1u << 30), r->events, r->failures, sent);
}

static void *receiver(void *unused) {
    (void)unused;
    for (;;) {
        InboxBuffer in;
        memset(&in, 0, sizeof in);
        kern_return_t kr = mach_msg(&in.m.h, MACH_RCV_MSG, 0, sizeof in, ctl_port, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
        if (kr != KERN_SUCCESS) {
            pthread_mutex_lock(&mutex);
            lost_locked(mach_error_string(kr));
            pthread_mutex_unlock(&mutex);
            return NULL;
        }
        if (in.m.h.msgh_id == BBGPU_MSG_VERIFY_REPLY && in.m.h.msgh_size >= sizeof(BbGpuVerifyReply)) {
            handle_reply(&in.m.reply);
        } else if (in.m.h.msgh_id == MACH_NOTIFY_DEAD_NAME) {
            int status = 0;
            char why[96] = "host process exited";
            if (host_is_child && waitpid(host_pid, &status, 0) == host_pid) {
                if (WIFEXITED(status)) snprintf(why, sizeof why, "host process exited with status %d", WEXITSTATUS(status));
                else if (WIFSIGNALED(status)) snprintf(why, sizeof why, "host process killed by signal %d", WTERMSIG(status));
            } else if (shared && __atomic_load_n(&shared->host_state, __ATOMIC_ACQUIRE) == 4u) {
                snprintf(why, sizeof why, "host process exited with status 0");
            }
            pthread_mutex_lock(&mutex);
            lost_locked(why);
            pthread_mutex_unlock(&mutex);
            return NULL;
        } else {
            mach_msg_destroy(&in.m.h);
        }
    }
}

static long elapsed_ms(const struct timespec *start) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec - start->tv_sec) * 1000 + (now.tv_nsec - start->tv_nsec) / 1000000;
}

/* Flushes event batches and runs the periodic verification. */
static void *monitor(void *unused) {
    (void)unused;
    const char *env = getenv("BB_GPU_PROCESS_VERIFY_MS");
    const long verify_ms = env ? strtol(env, NULL, 10) : 0; /* a diagnostic: off by default */
    struct timespec last;
    clock_gettime(CLOCK_MONOTONIC, &last);
    unsigned rotate = 0;
    for (;;) {
        struct timespec t = {0, FLUSH_MS * 1000000L};
        nanosleep(&t, NULL);
        pthread_mutex_lock(&mutex);
        const int live = alive;
        pthread_mutex_unlock(&mutex);
        if (!live) return NULL;
        if (verify_ms > 0 && elapsed_ms(&last) >= verify_ms) {
            clock_gettime(CLOCK_MONOTONIC, &last);
            runtime_memory_mirror_snapshot(rotate++, SAMPLE_BYTES, emit_verify);
        }
    }
}

/* Receives one message of the given id on port, watching the host for an early exit. */
static int receive(mach_port_t port, mach_msg_id_t id, InboxBuffer *in) {
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    while (elapsed_ms(&start) < HANDSHAKE_TIMEOUT_MS) {
        memset(in, 0, sizeof *in);
        kern_return_t kr = mach_msg(&in->m.h, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, sizeof *in, port, 100, MACH_PORT_NULL);
        if (kr == MACH_RCV_TIMED_OUT) {
            int status;
            if (host_is_child) {
                if (waitpid(host_pid, &status, WNOHANG) == host_pid) {
                    printf("GPU process: host exited during the handshake (status 0x%x)\n", status);
                    host_pid = 0;
                    return -1;
                }
            } else if (host_pid > 0 && kill(host_pid, 0) != 0 && errno == ESRCH) {
                printf("GPU process: host exited during the handshake\n");
                host_pid = 0;
                return -1;
            }
            continue;
        }
        if (kr != KERN_SUCCESS) { printf("GPU process: receive failed: %s\n", mach_error_string(kr)); return -1; }
        if (in->m.h.msgh_id == id) return 0;
        mach_msg_destroy(&in->m.h);
    }
    printf("GPU process: no answer from the host within %d ms\n", HANDSHAKE_TIMEOUT_MS);
    return -1;
}

/* The major macOS version (kern.osproductversion "27.0.1" -> 27), 0 if unknown. */
static int macos_major(void) {
    char version[32] = "";
    size_t size = sizeof version;
    if (sysctlbyname("kern.osproductversion", version, &size, NULL, 0)) return 0;
    return atoi(version);
}

static int host_path(char *out, size_t size) {
    const char *env = getenv("BB_GPU_HOST");
    if (env && *env) {
        snprintf(out, size, "%s", env);
    } else {
        char exe[PATH_MAX], real[PATH_MAX];
        uint32_t n = sizeof exe;
        if (_NSGetExecutablePath(exe, &n) || !realpath(exe, real)) return -1;
        snprintf(out, size, "%s/bbgpu-host", dirname(real));
    }
    /* macOS 27 and later: the host built with the guest range as an exec-time segment
     * (build_macos.sh, host/main.c reserve_guest_range), beside the plain one. Earlier macOS
     * keeps the plain host: there the pinned segment breaks dyld. */
    if (macos_major() >= 27) {
        char variant[PATH_MAX];
        snprintf(variant, sizeof variant, "%s-macos27", out);
        if (access(variant, X_OK) == 0) snprintf(out, size, "%s", variant);
    }
    return 0;
}

static int env_allowed(const char *entry) {
    /* Online secrets stay in this process: the host never talks to the network. */
    static const char *const withheld[] = {"BBGPU_HANDSHAKE=", "BBGPU_HANDSHAKE_SERVICE=",
                                           "BB_THD_TOKEN=", "BB_SHADNET_PASSWORD=",
                                           "BB_SHADNET_TOKEN="};
    for (size_t w = 0; w < sizeof withheld / sizeof *withheld; ++w)
        if (!strncmp(entry, withheld[w], strlen(withheld[w]))) return 0;
    return 1;
}

/* Enclosing .app bundle for a host binary in <bundle>/Contents/MacOS/bbgpu-host*. Returns 0 if
 * found and the bundle has Info.plist + the Bloodborne launcher trampoline. */
static int enclosing_app_bundle(const char *host, char *bundle, size_t size) {
    const char *needle = "/Contents/MacOS/";
    const char *hit = NULL, *scan = host;
    while ((scan = strstr(scan, needle)) != NULL) {
        hit = scan;
        scan += strlen(needle);
    }
    if (!hit) return -1;
    const size_t len = (size_t)(hit - host);
    if (len < 5 || len >= size || memcmp(hit - 4, ".app", 4) != 0) return -1;
    memcpy(bundle, host, len);
    bundle[len] = 0;
    char check[PATH_MAX];
    snprintf(check, sizeof check, "%s/Contents/Info.plist", bundle);
    if (access(check, R_OK) != 0) return -1;
    snprintf(check, sizeof check, "%s/Contents/MacOS/Bloodborne", bundle);
    if (access(check, X_OK) != 0) return -1;
    return 0;
}

static int send_all(int fd, const void *buf, size_t len) {
    const unsigned char *p = (const unsigned char *)buf;
    while (len > 0) {
        ssize_t n = send(fd, p, len, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        p += (size_t)n;
        len -= (size_t)n;
    }
    return 0;
}

/* Launches Bloodborne.app via LaunchServices (`open -n -g`) so RunningBoard and macOS Game Mode
 * associate bbgpu-host with Bloodborne.app's bundle ID and icon (instead of an anonymous process
 * with bundleID=nil and a blank Game Mode notification icon). Bloodborne's trampoline connects to
 * our private AF_UNIX socket, receives stdout/stderr and the sanitized environment, and execve()s
 * bbgpu-host(-macos27) in the same LaunchServices PID. Returns 0 on success, -1 to fall back. */
static int spawn_via_launch_services(const char *path, mach_port_t port) {
    const char *override = getenv("BB_GPU_HOST_LAUNCH_SERVICES");
    if (override && !strcmp(override, "0")) return -1;
    char bundle[PATH_MAX];
    if (enclosing_app_bundle(path, bundle, sizeof bundle) != 0) return -1;

    name_t service;
    snprintf(service, sizeof service, "io.github.wooloomooloo2.bloodborne-mac-port.gpu.%d.%08x",
             (int)getpid(), arc4random());
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    kern_return_t kr = bootstrap_register(bootstrap_port, service, port);
#pragma clang diagnostic pop
    if (kr != KERN_SUCCESS) {
        printf("GPU process: bootstrap_register failed (%s); falling back to direct spawn\n",
               mach_error_string(kr));
        return -1;
    }

    char dir[] = "/tmp/bbgpu-XXXXXX";
    if (!mkdtemp(dir)) return -1;
    char sock_path[sizeof(((struct sockaddr_un *)0)->sun_path)];
    snprintf(sock_path, sizeof sock_path, "%s/s", dir);

    int srv = socket(AF_UNIX, SOCK_STREAM, 0);
    int cli = -1, ok = 0;
    char *payload = NULL;
    if (srv < 0) goto cleanup;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof addr);
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path, sizeof addr.sun_path - 1);
    if (bind(srv, (struct sockaddr *)&addr, (socklen_t)(offsetof(struct sockaddr_un, sun_path) + strlen(sock_path) + 1)) != 0 ||
        listen(srv, 1) != 0) {
        goto cleanup;
    }

    posix_spawn_file_actions_t open_files;
    posix_spawn_file_actions_init(&open_files);
    posix_spawn_file_actions_addopen(&open_files, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_addopen(&open_files, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addopen(&open_files, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    char *open_argv[] = {"/usr/bin/open", "-n", "-g", bundle, "--args",
                         "--gpu-host-bootstrap", sock_path, NULL};
    pid_t open_pid = 0;
    int rc = posix_spawn(&open_pid, "/usr/bin/open", &open_files, NULL, open_argv, environ);
    posix_spawn_file_actions_destroy(&open_files);
    if (rc != 0) {
        printf("GPU process: /usr/bin/open failed (%s); falling back to direct spawn\n", strerror(rc));
        goto cleanup;
    }
    int open_status = 0;
    while (waitpid(open_pid, &open_status, 0) < 0 && errno == EINTR) {}
    if (!WIFEXITED(open_status) || WEXITSTATUS(open_status) != 0) {
        printf("GPU process: LaunchServices open exited with status 0x%x; falling back to direct spawn\n",
               open_status);
        goto cleanup;
    }

    struct pollfd pfd = {.fd = srv, .events = POLLIN};
    int ready;
    do { ready = poll(&pfd, 1, LAUNCH_CONNECT_TIMEOUT_MS); } while (ready < 0 && errno == EINTR);
    if (ready <= 0) {
        printf("GPU process: timed out waiting for app trampoline; falling back to direct spawn\n");
        goto cleanup;
    }
    cli = accept(srv, NULL, NULL);
    if (cli < 0) goto cleanup;
    int one = 1;
    setsockopt(cli, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one);

    uid_t peer_uid = 0;
    gid_t peer_gid = 0;
    if (getpeereid(cli, &peer_uid, &peer_gid) != 0 || peer_uid != geteuid()) goto cleanup;
    pid_t peer_pid = 0;
    socklen_t pid_len = sizeof peer_pid;
    if (getsockopt(cli, SOL_LOCAL, LOCAL_PEERPID, &peer_pid, &pid_len) != 0 || peer_pid <= 0) goto cleanup;

    char service_env[sizeof(name_t) + 32];
    snprintf(service_env, sizeof service_env, "BBGPU_HANDSHAKE_SERVICE=%s", service);

    size_t total = strlen(path) + 1 + strlen(service_env) + 1;
    for (size_t i = 0; environ[i]; ++i)
        if (env_allowed(environ[i])) total += strlen(environ[i]) + 1;
    if (total > 512 * 1024) goto cleanup;
    payload = malloc(total);
    if (!payload) goto cleanup;
    size_t pos = 0;
    size_t n = strlen(path) + 1;
    memcpy(payload + pos, path, n); pos += n;
    for (size_t i = 0; environ[i]; ++i) {
        if (!env_allowed(environ[i])) continue;
        n = strlen(environ[i]) + 1;
        memcpy(payload + pos, environ[i], n); pos += n;
    }
    n = strlen(service_env) + 1;
    memcpy(payload + pos, service_env, n); pos += n;

    uint32_t payload_len = (uint32_t)pos;
    int fds[2] = {STDOUT_FILENO, STDERR_FILENO};
    char cbuf[CMSG_SPACE(sizeof fds)];
    memset(cbuf, 0, sizeof cbuf);
    struct iovec iov = {.iov_base = &payload_len, .iov_len = sizeof payload_len};
    struct msghdr msg;
    memset(&msg, 0, sizeof msg);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cbuf;
    msg.msg_controllen = sizeof cbuf;
    struct cmsghdr *cm = CMSG_FIRSTHDR(&msg);
    cm->cmsg_level = SOL_SOCKET;
    cm->cmsg_type = SCM_RIGHTS;
    cm->cmsg_len = CMSG_LEN(sizeof fds);
    memcpy(CMSG_DATA(cm), fds, sizeof fds);

    ssize_t sent;
    do { sent = sendmsg(cli, &msg, 0); } while (sent < 0 && errno == EINTR);
    if (sent != (ssize_t)sizeof payload_len) goto cleanup;
    if (send_all(cli, payload, pos) != 0) goto cleanup;

    host_pid = peer_pid;
    host_is_child = 0;
    ok = 1;

cleanup:
    free(payload);
    if (cli >= 0) close(cli);
    if (srv >= 0) close(srv);
    unlink(sock_path);
    rmdir(dir);
    return ok ? 0 : -1;
}

/* posix_spawn with port inherited through mach_ports_register (not the bootstrap special port:
 * libxpc caches it, and the host needs launchd/XPC for Metal and the window server). The
 * registration is restored right after, so later spawns don't inherit it. */
static int spawn_direct(const char *path, mach_port_t port) {
    mach_port_array_t saved = NULL;
    mach_msg_type_number_t saved_count = 0;
    if (mach_ports_lookup(mach_task_self(), &saved, &saved_count) != KERN_SUCCESS) saved_count = 0;
    posix_spawnattr_t attr;
    posix_spawn_file_actions_t files;
    posix_spawnattr_init(&attr);
    posix_spawn_file_actions_init(&files);
    posix_spawnattr_setflags(&attr, POSIX_SPAWN_CLOEXEC_DEFAULT); /* no game files in the host */
    for (int fd = 0; fd < 3; ++fd) posix_spawn_file_actions_addinherit_np(&files, fd);
    cpu_type_t arm64 = CPU_TYPE_ARM64;
    size_t set = 0;
    posix_spawnattr_setbinpref_np(&attr, 1, &arm64, &set);
    char *argv[] = {(char *)path, NULL};
    /* BBGPU_HANDSHAKE tells the host the registered port is ours (registrations are inherited,
     * so a host started by hand could otherwise find a stranger's port there). */
    size_t count = 0;
    while (environ[count]) ++count;
    char **envp = calloc(count + 2, sizeof *envp), marker[64];
    snprintf(marker, sizeof marker, "BBGPU_HANDSHAKE=%d", (int)getpid());
    size_t used = 0;
    for (size_t i = 0; envp && i < count; ++i)
        if (env_allowed(environ[i])) envp[used++] = environ[i];
    if (envp) envp[used] = marker;
    int rc = ENOMEM;
    kern_return_t kr = mach_ports_register(mach_task_self(), &port, 1);
    if (kr == KERN_SUCCESS && envp) rc = posix_spawn(&host_pid, path, &files, &attr, argv, envp);
    free(envp);
    int any = 0;
    for (mach_msg_type_number_t i = 0; i < saved_count; ++i) any |= saved[i] != MACH_PORT_NULL;
    mach_ports_register(mach_task_self(), any ? saved : NULL, any ? saved_count : 0);
    for (mach_msg_type_number_t i = 0; i < saved_count; ++i)
        if (saved[i] != MACH_PORT_NULL) mach_port_deallocate(mach_task_self(), saved[i]);
    if (saved) vm_deallocate(mach_task_self(), (vm_address_t)saved, saved_count * sizeof(*saved));
    posix_spawn_file_actions_destroy(&files);
    posix_spawnattr_destroy(&attr);
    if (kr != KERN_SUCCESS) { printf("GPU process: mach_ports_register: %s\n", mach_error_string(kr)); return -1; }
    if (rc) { printf("GPU process: cannot start %s: %s\n", path, strerror(rc)); host_pid = 0; return -1; }
    host_is_child = 1;
    return 0;
}

static int spawn(const char *path, mach_port_t port) {
    if (spawn_via_launch_services(path, port) == 0) return 0;
    return spawn_direct(path, port);
}

static int host_still_alive(void) {
    if (host_pid <= 0) return 0;
    if (host_is_child) return waitpid(host_pid, NULL, WNOHANG) == 0;
    return kill(host_pid, 0) == 0 || errno == EPERM;
}

/* SIGTERM, then SIGKILL if the host has not exited within 3 s (a stuck host may never get to
 * its exit path, and the game must not wait on it forever). A host stuck in the kernel on hung
 * GPU work does not finish exiting even after SIGKILL: give it 2 s more, then go on (#71). */
static void stop_host(void) {
    if (host_pid > 0) {
        kill(host_pid, SIGTERM);
        for (int i = 0; i < 300 && host_still_alive(); ++i) usleep(10000);
        if (kill(host_pid, 0) == 0) {
            kill(host_pid, SIGKILL);
            int i = 0;
            while (i++ < 200 && host_still_alive()) usleep(10000);
            if (i > 200) printf("GPU process: host %d did not exit after SIGKILL (stuck GPU?)\n", host_pid);
        }
        host_pid = 0;
    }
}

void gpu_process_stop(void) {
    pthread_mutex_lock(&mutex);
    alive = 0; /* the host's exit is expected now: no "lost" report */
    pthread_mutex_unlock(&mutex);
    stop_host();
}

/* Button prompts: the renderer knows the game's key guide icons from this file. Called once the
 * game's folders are mounted, before the renderer starts in the host. */
void gpu_process_publish_files(void) {
    if (shared && runtime_file_translate("/app0/dvdroot_ps4/menu/common.tpf.dcx", shared->prompt_file,
                                         sizeof shared->prompt_file))
        shared->prompt_file[0] = 0;
}

void *gpu_process_shared(void) {
    pthread_mutex_lock(&mutex);
    void *result = alive ? shared : NULL;
    pthread_mutex_unlock(&mutex);
    return result;
}

void gpu_process_rendering(void) {
    pthread_mutex_lock(&mutex);
    rendering = alive;
    pthread_mutex_unlock(&mutex);
}

void gpu_process_start(void) {
    runtime_pad_set_shared(gpu_process_shared); /* mouse look (runtime_pad.c) */
    if (runtime_memory_create_pool() || runtime_memory_pool_entry() == MACH_PORT_NULL)
        start_failed("no shared memory entry for the game's memory");
    char path[PATH_MAX];
    if (host_path(path, sizeof path)) start_failed("bbgpu-host was not found next to bb-probe");
    mach_port_t hs = MACH_PORT_NULL;
    if (mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &hs) != KERN_SUCCESS ||
        mach_port_insert_right(mach_task_self(), hs, hs, MACH_MSG_TYPE_MAKE_SEND) != KERN_SUCCESS)
        start_failed("cannot allocate the handshake port");
    /* The control block: a small entry of its own (offsets only; mapped anywhere on each side). */
    memory_object_size_t shared_size = BBGPU_SHARED_SIZE;
    mach_vm_address_t shared_at = 0;
    if (mach_make_memory_entry_64(mach_task_self(), &shared_size, 0, MAP_MEM_NAMED_CREATE | VM_PROT_READ | VM_PROT_WRITE,
                                  &shared_entry, MACH_PORT_NULL) != KERN_SUCCESS ||
        mach_vm_map(mach_task_self(), &shared_at, BBGPU_SHARED_SIZE, 0, VM_FLAGS_ANYWHERE, shared_entry, 0, FALSE,
                    VM_PROT_READ | VM_PROT_WRITE, VM_PROT_READ | VM_PROT_WRITE, VM_INHERIT_NONE) != KERN_SUCCESS)
        start_failed("cannot create the shared control block");
    shared = (BbGpuShared *)(uintptr_t)shared_at;
    bbgpu_shared_init(shared);
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    InboxBuffer in;
    printf("GPU process: starting %s\n", path);
    int ok = !spawn(path, hs);
    /* 1. hello: the host's port arrives as the reply port */
    if (ok) ok = !receive(hs, BBGPU_MSG_HELLO, &in);
    if (ok && (in.m.hello.version != BBGPU_PROTOCOL_VERSION || in.m.h.msgh_remote_port == MACH_PORT_NULL ||
               (host_pid > 0 && in.m.hello.pid != (uint32_t)host_pid))) {
        printf("GPU process: host protocol %u (pid %u), expected %u (pid %d)\n", in.m.hello.version,
               in.m.hello.pid, BBGPU_PROTOCOL_VERSION, (int)host_pid);
        mach_msg_destroy(&in.m.h);
        ok = 0;
    }
    if (ok) host_port = in.m.h.msgh_remote_port;
    /* 2. setup: memory entry + control port (host -> runtime) */
    if (ok) ok = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &ctl_port) == KERN_SUCCESS;
    if (ok) {
        mach_port_limits_t limits = {MACH_PORT_QLIMIT_LARGE};
        mach_port_set_attributes(mach_task_self(), ctl_port, MACH_PORT_LIMITS_INFO, (mach_port_info_t)&limits,
                                 MACH_PORT_LIMITS_INFO_COUNT);
        BbGpuSetup s;
        memset(&s, 0, sizeof s);
        s.h.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0) | MACH_MSGH_BITS_COMPLEX;
        s.h.msgh_size = sizeof s;
        s.h.msgh_remote_port = host_port;
        s.h.msgh_id = BBGPU_MSG_SETUP;
        s.body.msgh_descriptor_count = 3;
        s.entry = (mach_msg_port_descriptor_t){.name = runtime_memory_pool_entry(),
                                               .disposition = MACH_MSG_TYPE_COPY_SEND, .type = MACH_MSG_PORT_DESCRIPTOR};
        s.control = (mach_msg_port_descriptor_t){.name = ctl_port, .disposition = MACH_MSG_TYPE_MAKE_SEND,
                                                 .type = MACH_MSG_PORT_DESCRIPTOR};
        s.shared = (mach_msg_port_descriptor_t){.name = shared_entry, .disposition = MACH_MSG_TYPE_COPY_SEND,
                                                .type = MACH_MSG_PORT_DESCRIPTOR};
        s.version = BBGPU_PROTOCOL_VERSION;
        s.pid = (uint32_t)getpid();
        s.entry_size = runtime_memory_pool_entry_size();
        s.guest_min = BBGPU_GUEST_MIN;
        s.guest_max = BBGPU_GUEST_MAX;
        kern_return_t kr = mach_msg(&s.h, MACH_SEND_MSG | MACH_SEND_TIMEOUT, sizeof s, 0, MACH_PORT_NULL,
                                    SEND_TIMEOUT_MS, MACH_PORT_NULL);
        if (kr != KERN_SUCCESS) { printf("GPU process: setup send: %s\n", mach_error_string(kr)); ok = 0; }
    }
    /* the handshake port has done its job */
    mach_port_deallocate(mach_task_self(), hs);
    mach_port_mod_refs(mach_task_self(), hs, MACH_PORT_RIGHT_RECEIVE, -1);
    /* 3. ready */
    if (ok) ok = !receive(ctl_port, BBGPU_MSG_READY, &in);
    if (ok && in.m.ready.status) {
        printf("GPU process: host setup failed (status %d)\n", in.m.ready.status);
        ok = 0;
    }
    if (ok) {
        mach_port_t previous = MACH_PORT_NULL;
        ok = mach_port_request_notification(mach_task_self(), host_port, MACH_NOTIFY_DEAD_NAME, 0, ctl_port,
                                            MACH_MSG_TYPE_MAKE_SEND_ONCE, &previous) == KERN_SUCCESS;
        if (!ok) puts("GPU process: host died during the handshake");
    }
    if (!ok) {
        stop_host();
        start_failed("the handshake with bbgpu-host failed");
    }
    printf("GPU process: host pid %d ready in %ld ms (%s, page %u bytes); guest entry 0x%" PRIx64
           " bytes mirrored at 0x%" PRIx64 "-0x%" PRIx64 "\n",
           (int)host_pid, elapsed_ms(&start), in.m.ready.native ? "native arm64" : "TRANSLATED", in.m.ready.page_size,
           runtime_memory_pool_entry_size(), BBGPU_GUEST_MIN, BBGPU_GUEST_MAX);
    bbgpu_split_attach(shared, host_pid);
    pthread_mutex_lock(&mutex);
    alive = 1;
    pthread_mutex_unlock(&mutex);
    pthread_t thread;
    pthread_create(&thread, NULL, receiver, NULL);
    pthread_detach(thread);
    runtime_memory_set_mirror(mirror_event); /* replays existing views */
    pthread_create(&thread, NULL, monitor, NULL);
    pthread_detach(thread);
}
#endif
