#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "gt7_telemetry.h"

static void write_f32_le(uint8_t *buf, size_t off, float val) {
    uint32_t u = 0;
    memcpy(&u, &val, sizeof(u));
    buf[off + 0] = (uint8_t)(u & 0xFF);
    buf[off + 1] = (uint8_t)((u >> 8) & 0xFF);
    buf[off + 2] = (uint8_t)((u >> 16) & 0xFF);
    buf[off + 3] = (uint8_t)((u >> 24) & 0xFF);
}

static void write_i32_le(uint8_t *buf, size_t off, int32_t val) {
    uint32_t u = (uint32_t)val;
    buf[off + 0] = (uint8_t)(u & 0xFF);
    buf[off + 1] = (uint8_t)((u >> 8) & 0xFF);
    buf[off + 2] = (uint8_t)((u >> 16) & 0xFF);
    buf[off + 3] = (uint8_t)((u >> 24) & 0xFF);
}

static void write_u32_le(uint8_t *buf, size_t off, uint32_t val) {
    buf[off + 0] = (uint8_t)(val & 0xFF);
    buf[off + 1] = (uint8_t)((val >> 8) & 0xFF);
    buf[off + 2] = (uint8_t)((val >> 16) & 0xFF);
    buf[off + 3] = (uint8_t)((val >> 24) & 0xFF);
}

static void write_u16_le(uint8_t *buf, size_t off, uint16_t val) {
    buf[off + 0] = (uint8_t)(val & 0xFF);
    buf[off + 1] = (uint8_t)((val >> 8) & 0xFF);
}

static void build_forza_324(uint8_t *pkt, int32_t drivetrain, float torque_nm, float power_w, int8_t steer_i8) {
    memset(pkt, 0, 324);
    write_i32_le(pkt, 0x00, 1);           /* is_race_on */
    write_f32_le(pkt, 0x08, 9200.0f);     /* EngineMaxRpm */
    write_f32_le(pkt, 0x0C, 950.0f);      /* EngineIdleRpm */
    write_f32_le(pkt, 0x10, 6850.0f);     /* CurrentEngineRpm */
    write_f32_le(pkt, 0x28, 48.0f);       /* VelZ */

    for (int w = 0; w < 4; ++w) {
        write_f32_le(pkt, 0x44 + w * 4, 0.55f);
        write_f32_le(pkt, 0x54 + w * 4, 0.08f);
        write_f32_le(pkt, 0x64 + w * 4, 145.0f);
        write_f32_le(pkt, 0xB4 + w * 4, 0.12f);
    }
    write_f32_le(pkt, 0x84, 0.15f);       /* FL in puddle */
    write_i32_le(pkt, 0xD4, 2048);        /* CarOrdinal */
    write_i32_le(pkt, 0xD8, 5);           /* S2 */
    write_i32_le(pkt, 0xDC, 998);         /* PI 998 */
    write_i32_le(pkt, 0xE0, drivetrain);  /* 0=FWD, 1=RWD, 2=AWD */

    size_t d = 244;
    write_f32_le(pkt, d + 0x00, 1250.0f);
    write_f32_le(pkt, d + 0x04, 18.0f);
    write_f32_le(pkt, d + 0x08, -850.0f);
    write_f32_le(pkt, d + 0x0C, 48.0f);   /* Speed 48 m/s */
    write_f32_le(pkt, d + 0x10, power_w);
    write_f32_le(pkt, d + 0x14, torque_nm);
    for (int w = 0; w < 4; ++w) {
        write_f32_le(pkt, d + 0x18 + w * 4, 185.0f); /* 85C */
    }
    write_f32_le(pkt, d + 0x28, 18.5f);   /* Boost PSI */
    write_f32_le(pkt, d + 0x2C, 0.65f);   /* Fuel */
    write_f32_le(pkt, d + 0x30, 48200.0f);/* DistanceTraveled */
    write_f32_le(pkt, d + 0x34, 88.425f);
    write_f32_le(pkt, d + 0x38, 89.120f);
    write_f32_le(pkt, d + 0x3C, 42.600f);
    write_u16_le(pkt, d + 0x44, 4);       /* Lap 5 */
    pkt[d + 0x46] = 2;                    /* Pos 2 */
    pkt[d + 0x47] = 230;                  /* Throttle ~90% */
    pkt[d + 0x4B] = 5;                    /* 5th gear */
    pkt[d + 0x4C] = (uint8_t)steer_i8;
}

static bool send_loopback_udp(uint16_t port, const void *data, size_t len) {
    int tx = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (tx < 0) return false;
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &dest.sin_addr);
    ssize_t sent = sendto(tx, data, len, 0, (struct sockaddr *)&dest, sizeof(dest));
    close(tx);
    return sent == (ssize_t)len;
}

static uint16_t bound_port_of(int fd) {
    struct sockaddr_in sa;
    socklen_t sl = sizeof(sa);
    if (fd >= 0 && getsockname(fd, (struct sockaddr *)&sa, &sl) == 0) {
        return ntohs(sa.sin_port);
    }
    return 0;
}

int main(void) {
    printf("=== GT7 & SIMHUB TELEMETRY + NETWORK STATE MACHINE TESTS ===\n");

    gt7_client_t client;
    gt7_state_t state;
    memset(&client, 0, sizeof(client));
    memset(&state, 0, sizeof(state));

    /* 1. Forza AWD + same-frame steer yaw bias */
    uint8_t fh_pkt[324];
    build_forza_324(fh_pkt, 2 /* AWD */, 680.0f, 485000.0f, 64);
    assert(simhub_parse_packet(&client, &state, fh_pkt, sizeof(fh_pkt), 1000));
    assert(strcmp(state.simhub_format, "SIMHUB FZA") == 0);
    assert(state.current_gear == 5);
    assert(fabsf(state.speed_kmh - 172.8f) < 0.1f);
    assert(fabsf(state.tire_temp[0] - 85.0f) < 1.0f);
    assert(strcmp(state.car_category, "S2 998") == 0);
    assert(state.surface_type[0] == 'S' && state.surface_type[1] == 'T');
    assert(fabsf(state.energy_recovery_kw - 485.0f) < 0.1f);
    assert(fabsf(state.session_distance_km - 48.2f) < 0.1f);
    assert(state.torque_vector_nm[1] > state.torque_vector_nm[0]); /* Right steer increases outer/right wheel calc */
    assert(state.torque_vector_nm[2] > state.torque_vector_nm[0]); /* AWD rear bias */
    printf("  [PASS] Test 1: Forza AWD 324B + same-frame steer bias + S2 998 PI + puddle\n");

    /* 2. Forza RWD & FWD must NOT be overwritten by simhub_post_process! */
    build_forza_324(fh_pkt, 1 /* RWD */, 500.0f, 350000.0f, 0);
    assert(simhub_parse_packet(&client, &state, fh_pkt, sizeof(fh_pkt), 1016));
    assert(fabsf(state.torque_vector_nm[0]) < 0.001f);
    assert(fabsf(state.torque_vector_nm[1]) < 0.001f);
    assert(fabsf(state.torque_vector_nm[2] - 250.0f) < 0.1f);
    assert(fabsf(state.torque_vector_nm[3] - 250.0f) < 0.1f);

    build_forza_324(fh_pkt, 0 /* FWD */, 360.0f, 220000.0f, 0);
    assert(simhub_parse_packet(&client, &state, fh_pkt, sizeof(fh_pkt), 1032));
    assert(fabsf(state.torque_vector_nm[0] - 180.0f) < 0.1f);
    assert(fabsf(state.torque_vector_nm[1] - 180.0f) < 0.1f);
    assert(fabsf(state.torque_vector_nm[2]) < 0.001f);
    assert(fabsf(state.torque_vector_nm[3]) < 0.001f);
    printf("  [PASS] Test 2: Forza RWD/FWD torque vectors preserved (not overwritten by fallback)\n");

    /* 3. Forza Pause zero-packet safeguard & resume clears GT7_FLAG_PAUSED */
    memset(fh_pkt, 0, sizeof(fh_pkt));
    assert(simhub_parse_packet(&client, &state, fh_pkt, sizeof(fh_pkt), 1048));
    assert(state.current_gear == -1);
    assert((state.flags & GT7_FLAG_PAUSED) != 0);

    build_forza_324(fh_pkt, 1, 400.0f, 280000.0f, 0);
    assert(simhub_parse_packet(&client, &state, fh_pkt, sizeof(fh_pkt), 1064));
    assert((state.flags & GT7_FLAG_PAUSED) == 0);
    assert(state.current_gear == 5);
    printf("  [PASS] Test 3: Forza zero-packet pause safeguard & unpause transition\n");

    /* 4. SimHub JSON preserves exact Torque (0-based or specific Nm) & Power */
    const char *json_pkt =
        "{\"Rpms\":7400,\"SpeedKmh\":215.4,\"Gear\":\"4\",\"Throttle\":88.0,\"Brake\":0.0,"
        "\"Handbrake\":1,\"PitLimiterOn\":1,\"DrsEnabled\":1,\"CarModel\":\"AMG GT3\","
        "\"Torque\":600.0,\"Power\":410000.0,\"TyreTemperatureFrontLeft\":82.5}";
    assert(simhub_parse_packet(&client, &state, (const uint8_t *)json_pkt, strlen(json_pkt), 1080));
    assert(state.current_gear == 4);
    assert((state.flags & GT7_FLAG_HANDBRAKE) != 0);
    assert((state.flags & GT7_FLAG_IN_PIT) != 0);
    assert(fabsf(state.energy_recovery_kw - 410.0f) < 0.1f);
    assert(fabsf(state.torque_vector_nm[0] - 120.0f) < 0.1f); /* 600 * 0.4 * 0.5 = 120 Nm */
    assert(fabsf(state.torque_vector_nm[2] - 180.0f) < 0.1f); /* 600 * 0.6 * 0.5 = 180 Nm */
    printf("  [PASS] Test 4: SimHub JSON exact Torque/Power/Handbrake/Pit/DRS preserved\n");

    /* 5. Network State Machine & Loopback Socket Tests */
    gt7_client_t net_c;
    gt7_state_t net_s;
    assert(gt7_init(&net_c, &net_s, "AUTO"));
    assert(strlen(net_c.local_ip) >= 7);
    assert(strlen(net_c.subnet_prefix) >= 5);

    /* 5a. Idle startup must NOT needlessly churn/recycle UDP sockets after 8+ seconds */
    gt7_update(&net_c, &net_s, 100);
    gt7_update(&net_c, &net_s, 10000);
    assert(net_c.last_socket_recycle_ms == 0);

    /* 5b. Lock Manual IP -> incoming SimHub UDP must NOT overwrite locked ps5_ip! */
    gt7_set_ip(&net_c, "192.168.0.165");
    assert(!net_c.auto_detect);
    assert(strcmp(net_c.ps5_ip, "192.168.0.165") == 0);

    uint16_t sh_port = bound_port_of(net_c.simhub_sock_fd);
    if (sh_port > 0) {
        assert(send_loopback_udp(sh_port, json_pkt, strlen(json_pkt)));
        usleep(10000);
        gt7_update(&net_c, &net_s, 10050);
        assert(net_s.connected);
        assert(net_s.is_simhub);
        assert(strcmp(net_c.simhub_ip, "127.0.0.1") == 0);
        assert(strcmp(net_c.ps5_ip, "192.168.0.165") == 0); /* LOCKED IP PROTECTED! */
        assert(!net_c.auto_detect);
    }

    /* 5c. Switch to AUTO -> receive native G7S0 PS5 packet -> timeout triggers direct reconnect -> >8s recycles */
    gt7_set_auto_detect(&net_c, &net_s);
    assert(net_c.auto_detect);
    assert(strcmp(net_c.ps5_ip, "AUTO") == 0);
    assert(net_c.last_ps5_seen_ms == 0);

    uint16_t gt7_port = bound_port_of(net_c.sock_fd);
    if (gt7_port > 0) {
        uint8_t gt7_raw[368];
        memset(gt7_raw, 0, sizeof(gt7_raw));
        write_u32_le(gt7_raw, 0x00, GT7_MAGIC_G7S0);
        write_f32_le(gt7_raw, 0x3C, 7100.0f);
        write_f32_le(gt7_raw, 0x4C, 50.0f);
        gt7_raw[0x90] = 0x54;
        gt7_raw[0x13C] = 'T';
        gt7_raw[0x13D] = 'C';
        gt7_raw[0x13E] = 'T';
        gt7_raw[0x13F] = 'T';
        memcpy(gt7_raw + 0x150, "GR3", 3);

        assert(send_loopback_udp(gt7_port, gt7_raw, sizeof(gt7_raw)));
        usleep(10000);
        gt7_update(&net_c, &net_s, 12000);
        assert(net_s.connected);
        assert(!net_s.is_simhub);
        assert(strcmp(net_c.ps5_ip, "127.0.0.1") == 0);
        assert(strcmp(net_c.last_known_ps5_ip, "127.0.0.1") == 0);
        assert(net_c.last_ps5_seen_ms == 12000);

        /* Simulate 3s Wi-Fi dropout -> must enter reconnecting_direct WITHOUT forgetting IP */
        gt7_update(&net_c, &net_s, 15000);
        assert(!net_s.connected);
        assert(net_c.reconnecting_direct);
        assert(strcmp(net_c.last_known_ps5_ip, "127.0.0.1") == 0);
        assert(net_c.last_socket_recycle_ms == 0); /* Still < 8s */

        /* Simulate >8s disconnect -> socket recycle triggers safely, then after >15s switches to gentle fallback */
        gt7_update(&net_c, &net_s, 28000);
        assert(net_c.last_socket_recycle_ms == 28000);
        assert(!net_c.reconnecting_direct);
    }

    gt7_close(&net_c);
    printf("  [PASS] Test 5: Network state machine, locked IP protection & socket healing verified\n");

    printf("\n=== ALL 5 TELEMETRY & NETWORK STATE MACHINE SUITES PASSED ===\n");
    return 0;
}
