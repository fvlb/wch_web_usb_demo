#define _POSIX_C_SOURCE 200809L
#include <libusb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>

struct bench;
struct slot {
    struct bench *bench;
    struct libusb_transfer *transfer;
    uint64_t order;
    int ready;
};
struct bench {
    libusb_device_handle *device;
    struct slot *slots;
    unsigned depth, request_bytes, mps, active;
    uint8_t *image;
    size_t size, used, reserved;
    uint64_t submitted, consumed, completions, len64, len512, other;
    int failed, stopping;
    int pattern;
    double finished;
};
static volatile sig_atomic_t interrupted;
static void stop_signal(int sig) { (void)sig; interrupted = 1; }
static double now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}
static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static int counters(libusb_device_handle *device, uint32_t *values)
{
    uint8_t bytes[20];
    int rc = libusb_control_transfer(device, 0xc0, 0x0c, 0, 0, bytes, sizeof(bytes), 2000);
    if (rc != 20) return -1;
    for (unsigned i = 0; i < 5; i++) values[i] = get32(bytes + i * 4);
    return 0;
}
static int verify_bmp(const uint8_t *p, size_t size, int pattern)
{
    if (size < 54 || p[0] != 'B' || p[1] != 'M' || get32(p + 2) != size ||
        get32(p + 10) != 54 || get32(p + 14) != 40 ||
        p[26] != 1 || p[27] || p[28] != 24 || p[29] || get32(p + 30)) return -1;
    uint32_t width = get32(p + 18), height = get32(p + 22);
    if (width < 2 || height < 2 || (uint64_t)width * height * 3 + 54 != size) return -1;
    if (pattern) {
        for (size_t i = 54; i < size; i++) {
            size_t offset = (i - 54) % 32768;
            uint8_t expected = (uint8_t)offset ^ (uint8_t)((offset / 512) * 61);
            if (p[i] != expected) { fprintf(stderr, "Static pattern mismatch at %zu\n", i); return -1; }
        }
        return 0;
    }
    for (uint32_t row = 0; row < height; row++) {
        uint32_t actual_row = height - 1 - row;
        for (uint32_t col = 0; col < width; col++) {
            uint8_t expected[] = {(uint8_t)(col * 255 / (width - 1)),
                                  (uint8_t)(actual_row * 255 / (height - 1)),
                                  (uint8_t)(col + actual_row)};
            for (unsigned channel = 0; channel < 3; channel++) {
                size_t pos = 54 + ((size_t)row * width + col) * 3 + channel;
                if (p[pos] != expected[channel]) {
                    fprintf(stderr, "BMP mismatch offset=%zu row=%u col=%u channel=%u got=%u expected=%u\n",
                            pos, row, col, channel, p[pos], expected[channel]);
                    return -1;
                }
            }
        }
    }
    return 0;
}
static int submit(struct slot *slot);
static void LIBUSB_CALL complete(struct libusb_transfer *transfer)
{
    struct slot *slot = transfer->user_data;
    struct bench *b = slot->bench;
    b->active--;
    if (b->stopping) return;
    slot->ready = 1;
    /* Drain in submission order, even if libusb delivers completions out of order. */
    for (;;) {
        struct slot *next = NULL;
        for (unsigned i = 0; i < b->depth; i++)
            if (b->slots[i].ready && b->slots[i].order == b->consumed) next = b->slots + i;
        if (!next) break;
        struct libusb_transfer *t = next->transfer;
        next->ready = 0;
        b->reserved -= (unsigned)t->length;
        b->consumed++;
        if (t->status != LIBUSB_TRANSFER_COMPLETED || t->actual_length <= 0 ||
            (size_t)t->actual_length > b->size - b->used) {
            fprintf(stderr, "Transfer failed status=%d actual=%d expected_remaining=%zu\n",
                    t->status, t->actual_length, b->size - b->used);
            b->failed = b->stopping = 1;
            return;
        }
        memcpy(b->image + b->used, t->buffer, (unsigned)t->actual_length);
        b->used += (unsigned)t->actual_length;
        b->completions++;
        if (t->actual_length == 64) b->len64++;
        else if (t->actual_length == 512) b->len512++;
        else b->other++;
        if (b->used == b->size) {
            b->finished = now(); b->stopping = 1;
            return;
        }
        if (submit(next)) { b->failed = b->stopping = 1; return; }
    }
}
static int submit(struct slot *slot)
{
    struct bench *b = slot->bench;
    if (b->used + b->reserved >= b->size) return 0;
    size_t available = b->size - b->used - b->reserved;
    unsigned length = available > b->request_bytes ? b->request_bytes : (unsigned)available;
    /* USB IN buffers must accommodate a whole packet; the final short packet
     * terminates the final request naturally. Never post reads past EOF. */
    length = (length + b->mps - 1) / b->mps * b->mps;
    libusb_fill_bulk_transfer(slot->transfer, b->device, 0x82, slot->transfer->buffer,
                             (int)length, complete, slot, 5000);
    slot->order = b->submitted++;
    int rc = libusb_submit_transfer(slot->transfer);
    if (rc) { fprintf(stderr, "submit: %s\n", libusb_error_name(rc)); return -1; }
    b->active++; b->reserved += length;
    return 0;
}
static int run(libusb_context *context, struct bench *b, unsigned iteration)
{
    uint32_t before[5] = {0}, after[5] = {0};
    b->used = b->reserved = b->submitted = b->consumed = b->completions = 0;
    b->active = b->len64 = b->len512 = b->other = 0;
    b->stopping = b->failed = 0; b->finished = 0;
    for (unsigned i = 0; i < b->depth; i++) b->slots[i].ready = 0;
    int have_before = counters(b->device, before) == 0;
    double start = now();
    int rc = libusb_control_transfer(b->device, 0x40, 0x0a, 0, 0, NULL, 0, 2000);
    if (rc) { fprintf(stderr, "START: %s\n", libusb_error_name(rc)); return -1; }
    /* Consume the header / first packet before queuing larger requests. */
    uint8_t first[512]; int received = 0;
    rc = libusb_bulk_transfer(b->device, 0x82, first, sizeof(first), &received, 5000);
    if (rc || received < 54 || (size_t)received > b->size) {
        fprintf(stderr, "First packet: %s length=%d\n", libusb_error_name(rc), received);
        return -1;
    }
    memcpy(b->image, first, (unsigned)received); b->used = (unsigned)received;
    for (unsigned i = 0; i < b->depth; i++)
        if (submit(b->slots + i)) { b->failed = 1; break; }
    while (b->active && !b->stopping && !b->failed && !interrupted && now() - start < 30) {
        struct timeval wait = {0, 100000};
        rc = libusb_handle_events_timeout_completed(context, &wait, NULL);
        if (rc && rc != LIBUSB_ERROR_INTERRUPTED) { b->failed = 1; break; }
    }
    b->stopping = 1;
    for (unsigned i = 0; i < b->depth; i++) libusb_cancel_transfer(b->slots[i].transfer);
    while (b->active) libusb_handle_events(context);
    int have_after = counters(b->device, after) == 0;
    double seconds = b->finished ? b->finished - start : now() - start;
    int valid = !b->failed && b->used == b->size && !verify_bmp(b->image, b->size, b->pattern);
    printf("{\"run\":%u,\"pattern\":\"%s\",\"queue\":%u,\"request_bytes\":%u,\"bytes\":%zu,\"seconds\":%.6f,"
           "\"MB_s\":%.3f,\"verified\":%s,\"completions\":%llu,\"len64\":%llu,\"len512\":%llu,"
           "\"other_lengths\":%llu,\"ep2_irqs\":%u,\"all_irqs\":%u}\n",
           iteration, b->pattern ? "static" : "gradient", b->depth, b->request_bytes, b->used, seconds, b->used / seconds / 1e6,
           valid ? "true" : "false", (unsigned long long)b->completions,
           (unsigned long long)b->len64, (unsigned long long)b->len512, (unsigned long long)b->other,
           have_before && have_after ? after[0] - before[0] : 0,
           have_before && have_after ? after[1] - before[1] : 0);
    fflush(stdout);
    return valid ? 0 : -1;
}
static unsigned number(const char *text, unsigned maximum)
{
    char *end;
    unsigned long n = strtoul(text, &end, 0);
    if (!*text || *end || !n || n > maximum) { fprintf(stderr, "Invalid number: %s\n", text); exit(2); }
    return (unsigned)n;
}
int main(int argc, char **argv)
{
    if (argc > 5 || (argc > 1 && !strcmp(argv[1], "--help"))) {
        fprintf(stderr, "Usage: %s [queue=16] [request_bytes=65536] [runs=3] [pattern]\n", argv[0]);
        return argc > 5 ? 2 : 0;
    }
    struct bench b = {0};
    b.depth = argc > 1 ? number(argv[1], 64) : 16;
    b.request_bytes = argc > 2 ? number(argv[2], 1024 * 1024) : 65536;
    unsigned runs = argc > 3 ? number(argv[3], 100) : 3;
    if (argc > 4) {
        if (strcmp(argv[4], "pattern")) { fprintf(stderr, "Fourth argument must be 'pattern'\n"); return 2; }
        b.pattern = 1;
    }
    libusb_context *context;
    if (libusb_init(&context)) return 1;
    int result = 1, claimed = 0;
    b.device = libusb_open_device_with_vid_pid(context, 0x1209, 1);
    if (!b.device) { fprintf(stderr, "No accessible demo 1209:0001\n"); goto cleanup; }
    int speed = libusb_get_device_speed(libusb_get_device(b.device));
    b.mps = speed == LIBUSB_SPEED_HIGH ? 512 : 64;
    fprintf(stderr, "USB speed=%s, EP2 MPS=%u\n", speed == LIBUSB_SPEED_HIGH ? "480M" : "12M", b.mps);
    if (b.request_bytes % b.mps) { fprintf(stderr, "request_bytes must be a multiple of MPS\n"); goto cleanup; }
    uint8_t size[4];
    if (libusb_control_transfer(b.device, 0xc0, 9, 0, 0, size, sizeof(size), 2000) != 4) goto cleanup;
    b.size = get32(size);
    if (b.size < 54 || b.size > 64 * 1024 * 1024) goto cleanup;
    if (libusb_claim_interface(b.device, 0)) goto cleanup;
    claimed = 1;
    b.image = malloc(b.size); b.slots = calloc(b.depth, sizeof(*b.slots));
    if (!b.image || !b.slots) goto cleanup;
    for (unsigned i = 0; i < b.depth; i++) {
        b.slots[i].bench = &b;
        b.slots[i].transfer = libusb_alloc_transfer(0);
        if (!b.slots[i].transfer) goto cleanup;
        b.slots[i].transfer->buffer = malloc(b.request_bytes);
        if (!b.slots[i].transfer->buffer) goto cleanup;
    }
    signal(SIGINT, stop_signal); signal(SIGTERM, stop_signal);
    result = 0;
    for (unsigned i = 0; i < runs && !interrupted; i++) {
        if (run(context, &b, i + 1)) { result = 1; break; }
    }
cleanup:
    if (b.slots) {
        for (unsigned i = 0; i < b.depth; i++) if (b.slots[i].transfer) {
            free(b.slots[i].transfer->buffer); libusb_free_transfer(b.slots[i].transfer);
        }
        free(b.slots);
    }
    free(b.image);
    if (b.device) { if (claimed) libusb_release_interface(b.device, 0); libusb_close(b.device); }
    libusb_exit(context);
    return interrupted ? 130 : result;
}
