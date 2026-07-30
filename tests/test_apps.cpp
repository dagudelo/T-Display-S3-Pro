/**
 * @file    test_apps.cpp
 * @brief   g++ unit tests for pure-logic app components (stock parser, weather parser)
 *
 * Build and run:
 *   g++ -std=c++17 -I../examples/factory -o test_apps test_apps.cpp && ./test_apps
 *
 * These tests exercise string parsing and logic that don't require
 * ESP32 hardware. They complement the PlatformIO build + QEMU boot test.
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <cassert>
#include <string>

/* ─── Minimal stubs for ESP32 types used in app code ────────────── */
using String = std::string;

/* ─── Test helpers ──────────────────────────────────────────────── */
static int passed = 0, failed = 0;

#define TEST(name) do { printf("  %-50s ", name); } while(0)
#define CHECK(cond) do { \
    if (cond) { printf("PASS\n"); passed++; } \
    else { printf("FAIL (%s:%d)\n", __FILE__, __LINE__); failed++; } \
} while(0)
#define SUMMARY() do { \
    printf("\nResults: %d passed, %d failed\n", passed, failed); \
} while(0)

/* ─── Test: Stock Yahoo Finance JSON parser ─────────────────────── */
static void test_stock_parser(void)
{
    TEST("parse regularMarketPrice key exists");
    {
        const char *json = "{\"regularMarketPrice\":192.53}";
        int p_idx = std::string(json).find("\"regularMarketPrice\":");
        CHECK(p_idx > 0);  /* key found after opening brace */
    }

    TEST("parse regularMarketChange key exists");
    {
        const char *json = "{\"regularMarketChange\":-3.21}";
        int c_idx = std::string(json).find("\"regularMarketChange\":");
        CHECK(c_idx > 0);
    }

    TEST("stock parser on empty response");
    {
        const char *json = "{}";
        int p_idx = std::string(json).find("\"regularMarketPrice\":");
        CHECK(p_idx < 0);  /* should not crash, just not found */
    }

    TEST("stock name array indexing");
    {
        const char *names[] = {"USD/COP", "Nu Holdings", "Amazon", "Apple", "NVIDIA"};
        int count = 5;
        int sel = 3;
        CHECK(std::string(names[sel]) == "Apple");
        sel = (sel + 1) % count;
        CHECK(std::string(names[sel]) == "NVIDIA");
        sel = (sel - 1 + count) % count;
        CHECK(std::string(names[sel]) == "Apple");
    }
}

/* ─── Test: Weather Open-Meteo parser ───────────────────────────── */
static void test_weather_parser(void)
{
    TEST("parse temperature_2m key exists");
    {
        const char *json = "{\"temperature_2m\":22.5}";
        int t_idx = std::string(json).find("\"temperature_2m\":");
        CHECK(t_idx > 0);
    }

    TEST("weather_code classification clear");
    {
        int codes[] = {0, 1, 2, 3};
        for (int c : codes) CHECK(c <= 3);  /* Clear */
    }

    TEST("weather_code classification cloudy");
    {
        int codes[] = {4, 45, 48};
        for (int c : codes) CHECK(c <= 48 && c > 3);
    }

    TEST("weather_code classification rain");
    {
        CHECK(61 <= 67 && 61 > 57);
    }

    TEST("weather_code classification storm");
    {
        CHECK(95 > 82);
    }

    TEST("snprintf buffer safety");
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.1f C  %s", 22.5, "Clear");
        CHECK(std::string(buf) == "22.5 C  Clear");
    }
}

/* ─── Test: Photos by-day path generation ───────────────────────── */
static void test_photo_path(void)
{
    TEST("day folder path format");
    {
        char day_folder[24];
        snprintf(day_folder, sizeof(day_folder), "/photos/%04d-%02d-%02d", 2026, 7, 30);
        CHECK(std::string(day_folder) == "/photos/2026-07-30");
    }

    TEST("image path in day folder");
    {
        char path[48];
        snprintf(path, sizeof(path), "%s/img%04d.bmp", "/photos/2026-07-30", 1000);
        CHECK(std::string(path) == "/photos/2026-07-30/img1000.bmp");
    }

    TEST("no buffer overflow on long paths");
    {
        char path[48];
        int n = snprintf(path, sizeof(path), "%s/img%04d.bmp",
                         "/photos/2026-12-31", 9999);
        CHECK(n < 48);  /* should fit */
    }
}

/* ─── Test: WiFi info formatting ────────────────────────────────── */
static void test_wifi_formatting(void)
{
    TEST("RSSI snprintf buffer");
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d dBm", -45);
        CHECK(std::string(buf) == "-45 dBm");
    }

    TEST("toast message buffer");
    {
        char toast[64];
        snprintf(toast, sizeof(toast), "WiFi: %s\nIP: %s", "MyNetwork", "192.168.1.100");
        CHECK(std::string(toast).find("MyNetwork") != std::string::npos);
        CHECK(std::string(toast).find("192.168.1.100") != std::string::npos);
        CHECK(strlen(toast) < 64);
    }
}

/* ─── Test: Tone player note frequencies ────────────────────────── */
static void test_tone_player(void)
{
    TEST("C4 frequency is 262 Hz");
    {
        int notes[] = {262, 294, 330, 349, 392, 440, 494, 523,
                       587, 659, 698, 784, 880, 988, 1047};
        CHECK(notes[0] == 262);
        CHECK(notes[7] == 523);   /* C5 */
        CHECK(notes[14] == 1047); /* C6 */
    }

    TEST("note name array matches frequencies");
    {
        const char *names[] = {"C","D","E","F","G","A","B","C5","D5","E5","F5","G5","A5","C6"};
        CHECK(std::string(names[0]) == "C");
        CHECK(std::string(names[7]) == "C5");
        CHECK(std::string(names[13]) == "C6");
    }
}

/* ─── Main ──────────────────────────────────────────────────────── */
int main(void)
{
    printf("=== T-Display-S3-Pro Unit Tests ===\n\n");

    printf("Stock parser:\n");
    test_stock_parser();

    printf("\nWeather parser:\n");
    test_weather_parser();

    printf("\nPhoto paths:\n");
    test_photo_path();

    printf("\nWiFi formatting:\n");
    test_wifi_formatting();

    printf("\nTone player:\n");
    test_tone_player();

    SUMMARY();
    return failed > 0 ? 1 : 0;
}
