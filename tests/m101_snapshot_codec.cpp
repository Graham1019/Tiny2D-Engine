#include "M5Test.h"
#include "tiny2d_game/NavigationSession.h"
#include "tiny2d_game/SnapshotCodec.h"
#include "../game/src/BinaryCodec.h"
#include <unistd.h>

using namespace tiny2d_game;
using binary::Bytes;

namespace {
void wireChecks() {
    NavigationSession live;
    for (int i=0; i<39; ++i) live.step(tiny2d::mapWalkInput(i));
    const auto source = live.captureSnapshot();
    const auto good = encodeSnapshot(source);
    auto decoded = decodeSnapshot(good);
    auto restored = NavigationSession::fromSnapshot(decoded);
    check(firstDifference(source,restored.captureSnapshot()).empty(), "exact restored fields");
    check(encodeSnapshot(restored.captureSnapshot()) == good, "canonical roundtrip");
    check(stateHash(source) == binary::Reader(good.data()+16,8).u64(), "canonical hash");
    ++decoded.enemies[1].position.x;
    check(firstDifference(source,decoded) == "enemies[1].position.x", "indexed field diagnostic");

    for (std::size_t length=0; length<good.size(); ++length)
        rejects([&]{ decodeSnapshot(Bytes(good.begin(),good.begin()+length)); });
    rejects([&]{ decodeSnapshot(Bytes(maxSnapshotBytes+1)); });
    for (auto index : {0,4,8,12,16,24}) {
        auto bad=good;
        bad[index]^=1;
        rejects([&]{ decodeSnapshot(bad); });
    }

    Bytes body(good.begin()+24,good.end());
    const auto tiles=source.world.tiles.size();
    const auto damageFlag=28+tiles+16+24+1+8+2+8+1+4+8+8+4+4;
    const auto flag=28+tiles+16+24;
    const auto enemyCount=damageFlag+1+8;
    check(!source.lastDamageSlot && body[damageFlag]==0 && body[enemyCount]==16, "test wire offsets");
    auto invalidByte=[&](std::size_t offset, std::uint8_t value) {
        auto bad=body;
        bad.at(offset)=value;
        rejects([&]{decodeSnapshot(binary::wrap("T2SN",snapshotFormatVersion,bad));});
    };
    invalidByte(24,255);          // dimension/count mismatch
    invalidByte(28,8);            // tile enum
    invalidByte(flag,2);          // boolean
    invalidByte(flag+19,9);       // phase
    invalidByte(damageFlag,255);  // optional tag
    invalidByte(enemyCount+3,255);
    for (auto bits : {0x7f800000u,0x7fc00000u}) {
        auto bad=body;
        for (unsigned i=0;i<4;++i) bad[flag+1+i]=static_cast<std::uint8_t>(bits>>(8*i));
        rejects([&]{decodeSnapshot(binary::wrap("T2SN",snapshotFormatVersion,bad));});
    }
    auto semanticallyInvalid=source;
    semanticallyInvalid.hp=100;
    const auto corrupt=encodeSnapshot(semanticallyInvalid); // recomputed, valid checksum
    rejects([&]{live.restoreSnapshot(decodeSnapshot(corrupt));});
    check(encodeSnapshot(live.captureSnapshot())==good, "semantic rejection preserves live world");
    body.push_back(0);
    rejects([&]{decodeSnapshot(binary::wrap("T2SN",snapshotFormatVersion,body));});
    body.resize(60);
    rejects([&]{decodeSnapshot(binary::wrap("T2SN",snapshotFormatVersion,body));});
}

void diskChecks() {
    auto pattern=(std::filesystem::temp_directory_path()/"tiny2d-codec-XXXXXX").string();
    if (!mkdtemp(pattern.data())) throw std::runtime_error("could not create test directory");
    struct Directory {
        std::filesystem::path path;
        ~Directory(){ std::error_code ec; std::filesystem::remove_all(path,ec); }
    } d{pattern};
    const auto name=d.path/"saved";
    const Bytes expected{0,1,42,255};
    writeNewFile(name,expected);
    rejects([&]{writeNewFile(name,{7});});
    check(readBoundedFile(name,4)==expected, "existing file survived collision");
    rejects([&]{readBoundedFile(name,3);});
    rejects([&]{readBoundedFile(d.path,1000);});
    rejects([&]{writeNewFile(d.path/"absent"/"snapshot",expected);});
    std::filesystem::create_symlink(name,d.path/"link");
    rejects([&]{writeNewFile(d.path/"link",{1});});
    check(readBoundedFile(name,4)==expected,"no-clobber symlink");
    check(std::distance(std::filesystem::directory_iterator(d.path),{})==2,"temporary removed after failure");
}
}

int main() {
    try { wireChecks(); diskChecks(); }
    catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
    std::cout<<"exact roundtrip, malformed and checksummed fields, failed restore, atomic create-only I/O passed\n";
}
