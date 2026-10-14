// Headless checks for the pure port logic: JSON reader, cheat registry, mod manifest, widescreen widths, and the TARGET_PC host conversions
// of the game code (tests/sms_host_checks.cpp, built with the sms include path).
// Build and run with tools/port_tests.cmd. Exit code is the number of failed checks.
#include <cstdio>
#include <string>
#include <vector>

#include "cheats/cheats_core.h"
#include "json/json_lite.h"
#include "mods/mod_manifest.h"
#include "settings.h"
#include "widescreen/widescreen.h"

namespace {

int gFailures = 0;
int gChecks = 0;

void check(bool ok, const char* what) {
    ++gChecks;
    if (!ok) {
        ++gFailures;
        std::printf("FAIL: %s\n", what);
    }
}

// Records what the effects asked for, so tick() can be checked without a game.
class FakeWorld : public cheats::World {
public:
    bool present = true;
    int healthRefills = 0;
    int waterRefills = 0;
    int flagSets = 0;
    std::vector<uint32_t> flags;

    bool marioPresent() override { return present; }
    void refillHealth() override { ++healthRefills; }
    void refillWater() override { ++waterRefills; }
    int32_t getFlag(uint32_t) override { return 0; }
    void setFlag(uint32_t flag, int32_t) override {
        ++flagSets;
        flags.push_back(flag);
    }
};

const char* kBaseJson = R"({
  "cheats": [
    {"id": "a", "name": "A", "category": "Mario", "effect": "keep_health_full", "default": false},
    {"id": "b", "name": "B", "category": "FLUDD", "effect": "keep_water_full", "default": false},
    {"id": "n", "name": "N", "category": "FLUDD", "effect": "unlock_all_nozzles", "default": false}
  ]
})";

void testJson() {
    json::Value v;
    std::string err;
    check(json::parse(R"({"x": [1, true, null, "s\u00e9\ud83d\ude00"]})", v, err), "json: valid input parses");
    const json::Value* x = v.find("x");
    check(x && x->type == json::Value::Type::Array && x->array.size() == 4, "json: array member");
    check(x && x->array[3].string == "s\xC3\xA9\xF0\x9F\x98\x80", "json: \\u escapes and surrogate pair");

    check(!json::parse("{\n  \"a\": 1,\n  oops\n}", v, err), "json: rejects bare word");
    check(err.rfind("3:", 0) == 0, "json: error reports line 3");
}

void testRegistry() {
    cheats::Registry reg;
    std::string err;
    check(reg.loadBase(kBaseJson, err), "registry: base loads");
    check(reg.list().size() == 3, "registry: three cheats");
    check(!reg.healthLossBlocked(), "registry: nothing enabled blocks health loss");

    check(reg.set("a", true) && reg.healthLossBlocked(), "registry: keep_health_full blocks health loss");
    check(reg.set("a", false) && !reg.healthLossBlocked(), "registry: disabling unblocks health loss");
    check(!reg.set("missing", true), "registry: set on unknown id fails");

    cheats::Registry bad;
    check(!bad.loadBase(R"({"cheats":[{"id":"x","name":"X","effect":"no_such_effect"}]})", err),
          "registry: unknown effect rejected");

    cheats::Registry user;
    check(user.loadBase(kBaseJson, err), "registry: base for user test");
    check(user.applyUser(R"({"cheats":[{"id":"a","name":"Renamed"}]})", err), "registry: user override accepted");
    check(user.find("a") && user.find("a")->name == "Renamed", "registry: user override changes name");
    check(!user.applyUser(R"({"cheats":[{"id":"new","name":"New"}]})", err),
          "registry: new user cheat without effect rejected");
}

void testTick() {
    cheats::Registry reg;
    std::string err;
    reg.loadBase(kBaseJson, err);

    FakeWorld world;
    reg.tick(world);
    check(world.healthRefills == 0 && world.waterRefills == 0, "tick: disabled cheats do nothing");

    reg.set("a", true);
    reg.set("b", true);
    reg.tick(world);
    check(world.healthRefills == 1, "tick: keep_health_full refills once per tick");
    check(world.waterRefills == 1, "tick: keep_water_full refills once per tick");

    world.present = false;
    reg.tick(world);
    check(world.healthRefills == 1 && world.waterRefills == 1, "tick: no refill without Mario");
    world.present = true;

    reg.set("n", true);
    reg.tick(world);
    check(world.flagSets == 18, "tick: unlock_all_nozzles sets 18 nozzle flags");
    check(world.flags.size() == 18 && world.flags.front() == 0x10366 && world.flags.back() == 0x10377,
          "tick: nozzle flags are the sms indices 0x10366..0x10377");
    check(reg.find("n") && !reg.find("n")->enabled, "tick: one-shot cheat switches itself off");
    const int setsAfterOneShot = world.flagSets;
    reg.tick(world);
    check(world.flagSets == setsAfterOneShot, "tick: one-shot cheat does not run again");
}

void testManifest() {
    mods::Manifest m;
    std::string err;
    check(mods::parseManifest(R"({"id":"demo-1","sunshine_api":"1.0","load_order":3})", m, err),
          "manifest: valid parses");
    check(m.id == "demo-1" && m.api.major == 1 && m.api.minor == 0 && m.loadOrder == 3,
          "manifest: fields read");
    check(!mods::parseManifest(R"({"sunshine_api":"1.0"})", m, err), "manifest: missing id rejected");
    check(!mods::parseManifest(R"({"id":"bad id!","sunshine_api":"1.0"})", m, err), "manifest: bad id rejected");
    check(!mods::parseManifest(R"({"id":"x"})", m, err), "manifest: missing sunshine_api rejected");
    check(!mods::parseManifest(R"({"id":"x","sunshine_api":"1.0","load_order":1e10})", m, err),
          "manifest: out-of-range load_order rejected");
    check(!mods::parseManifest(R"({"id":"x","sunshine_api":"1.0","load_order":1.5})", m, err),
          "manifest: fractional load_order rejected");

    std::string why;
    check(mods::apiCompatible({1, 0}, mods::kHostApi, why), "api: same version compatible");
    check(!mods::apiCompatible({2, 0}, mods::kHostApi, why), "api: different major rejected");
    check(!mods::apiCompatible({1, 5}, mods::kHostApi, why), "api: newer minor rejected");

    std::vector<mods::Manifest> list(2);
    list[0].id = "b";
    list[0].loadOrder = 0;
    list[1].id = "a";
    list[1].loadOrder = 0;
    mods::sortLoadOrder(list);
    check(list[0].id == "a" && list[1].id == "b", "sort: ties broken by id");

    std::vector<std::string> replaced;
    std::vector<mods::OverlayFile> in = {
        {"/sb/house.bin", "C:/m1/house.bin", 10, "m1"},
        {"/SB/House.bin", "C:/m2/house.bin", 12, "m2"},
    };
    std::vector<mods::OverlayFile> out = mods::mergeOverlays(in, replaced);
    check(out.size() == 1 && out[0].modId == "m2", "overlay: later mod wins, case-insensitive");
    check(replaced.size() == 1, "overlay: replacement reported");
    check(mods::overlayDvdPath("sb/x.bin") == "/sb/x.bin", "overlay: DVD path form");
}

void testWidescreen() {
    check(widescreenRenderWidthForAspect(4.0 / 3.0) == 640, "widescreen: 4:3 -> 640");
    check(widescreenRenderWidthForAspect(16.0 / 9.0) == 852, "widescreen: 16:9 -> 852 (multiple of 4)");
    check(widescreenRenderWidthForAspect(21.0 / 9.0) == 1120, "widescreen: 21:9 -> 1120");
    check(widescreenRenderWidthForAspect(32.0 / 9.0) == 1708, "widescreen: 32:9 -> 1708");
    check(widescreenRenderWidthForAspect(16.0 / 10.0) == 768, "widescreen: 16:10 -> 768");

    sunshine::Settings s;  // widescreenInit reads only .widescreen and .aspect (and SUNSHINE_ASPECT, unset here)
    s.widescreen = true;
    s.aspect = "auto";
    check(widescreenInit(s, 1281, 960) == 1281 && sunshineRenderWidth() == 640 && sunshineUIOffsetX() == 0,
          "widescreen: auto 1281x960 stays on the 640 path");
    s.aspect = "16:9";
    check(widescreenInit(s, 1280, 960) == 1704 && sunshineRenderWidth() == 852, "widescreen: 16:9 window 1704x960");
    check(sunshineUIOffsetX() == 106 && sunshineRenderWidth() - 640 - sunshineUIOffsetX() == 106,
          "widescreen: 16:9 UI column 106 px from both edges");
    s.aspect = "21:9";
    check(widescreenInit(s, 1280, 960) == 2240, "widescreen: 21:9 window 2240x960");
    s.widescreen = false;
    check(widescreenInit(s, 1280, 960) == 1280 && sunshineRenderWidth() == 640, "widescreen: off -> 640, window unchanged");
}

}  // namespace

void smsHostChecks(void (*check)(bool, const char*));

int main() {
    testJson();
    testRegistry();
    testTick();
    testManifest();
    testWidescreen();
    smsHostChecks(check);
    std::printf("%d checks, %d failed\n", gChecks, gFailures);
    return gFailures;
}
