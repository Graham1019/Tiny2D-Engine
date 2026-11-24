#include "Metrics.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string_view>

namespace lab {
using tiny2d::QueryStatus;
namespace {
// Everything through fullscreen is the published M7.4b schema. Retired GPU
// frames leave these CPU counters empty, even when new passes are appended.
const std::string& legacyCpuColumns() {
    static const std::string header=[] {
        std::ostringstream columns;
        columns<<",update_us,prepare_us,gui_build_us,query_poll_us,passes,draws,world_sprites,batch_vertex_bytes,gui_vertices,gui_indices,gui_vertex_bytes,gui_index_bytes,font_upload_bytes,hdr_bytes,preview_bytes,font_bytes";
        for (unsigned i=0;i<=ShadowInspect;++i) columns<<','<<passNames[i]<<"_draws";
        for (unsigned i=0;i<=ShadowInspect;++i) columns<<','<<passNames[i]<<"_batch_bytes";
        columns<<",light_extract_us,active_lights,dropped_lights,light_w,light_h,light_uniform_bytes"
            <<",normal_mapping,normal_material,normal_strength,normal_w,normal_h,normal_prepare_us,material_bytes,material_upload_bytes"
            <<",shadows,shadow_light_budget,occluders,active_casters,skipped_casters,occluder_count,edge_count"
            <<",mask_rebuilds,mask_draw_calls,masked_light_draw_calls,inspect_rebuilds,inspect_draw_calls"
            <<",geometry_us,mask_submit_us,light_submit_us,shadow_vertex_bytes,shadow_bytes,fullscreen";
        return columns.str();
    }();
    return header;
}
constexpr std::string_view particleCounters =
    ",active_particles,dropped_particles,active_fields,force_update_us,snapshot_prepare_us"
    ",instance_upload_us,instance_upload_bytes,active_instances,particle_draw_calls,valid_gpu_samples";
constexpr std::string_view mapCounters =
    ",map_generation_us,bfs_validation_us,collision_us,visible_tile_prepare_us,total_tiles,visible_tiles"
    ",submitted_tile_quads,tile_draw_calls,tile_vertex_upload_bytes,camera_x,camera_y,player_x,player_y,hit_x,hit_y";
constexpr std::string_view navCounters =
    ",nav_requested_enemies,nav_active_enemies,nav_phase,nav_hp,nav_max_hp,nav_player_speed,nav_enemy_speed,nav_guard_ticks"
    ",nav_offer_serial,nav_choice_serial,nav_last_choice,nav_target_x,nav_target_y,nav_field_rebuild_count,nav_cache_reuse_count"
    ",nav_visited_nodes,nav_expanded_nodes,nav_graph_build_events,nav_field_build_events,nav_graph_build_us,nav_field_build_us"
    ",nav_enemy_update_us,nav_enemy_collision_us,nav_upgrade_update_us,submitted_enemy_quads,enemy_draw_calls,enemy_vertex_upload_bytes";
std::string blanks(std::string_view header) {
    return std::string(std::count(header.begin(),header.end(),','),',');
}
void runConfig(std::ostream& out,const FrameInfo& f) {
    out<<','<<f.seed<<','<<(f.replay?"replay":f.navigation?"navigation":f.mapWidth?"map":f.galleryEffect?"shaders":f.particles?"particles":"neon")<<',';
    if (f.particles) {
        out<<(f.demoScript?"demo":"none")<<','<<f.particleCapacity<<','<<f.particleRate<<',';
        if (f.particleSteady) out<<*f.particleSteady;
    } else out<<",,,";
}
void replayConfig(std::ostream& out,const FrameInfo& f) {
    if (f.replay) {
        out<<','<<f.revision<<','<<f.eventSequence<<','<<f.recordedEvents<<','<<f.verifiedEvents
           <<','<<std::hex<<f.simulationHash<<std::dec<<','<<f.replayMode;
    } else out<<",,,,,,";
}
void mapConfig(std::ostream& out,const FrameInfo& f) {
    if (f.mapWidth) out<<','<<f.mapWidth<<','<<f.mapHeight<<','<<(f.mapWalk?"walk":"none");
    else out<<",,,";
}
void galleryConfig(std::ostream& out,const FrameInfo& f) {
    out<<',';
    if (f.galleryEffect) out<<galleryName(*f.galleryEffect);
    out<<',';
    if (f.galleryEffect) out<<f.shaderVersion;
}
const char* status(QueryStatus s) {
    switch(s) {
    case QueryStatus::Off: return "off_or_unavailable";
    case QueryStatus::Pending: return "pending";
    case QueryStatus::Skipped: return "skipped";
    case QueryStatus::Invalid: return "invalid";
    case QueryStatus::Complete: return "completed";
    }
    throw std::logic_error("unknown query status");
}
}
Metrics::Metrics(const std::string& path) {
    if (path.empty()) return;
    retired_.reserve(9);
    out_.exceptions(std::ios::badbit|std::ios::failbit);
    try { out_.open(path); }
    catch (const std::ios_base::failure& e) { throw std::runtime_error("open metrics "+path+": "+e.what()); }
    out_<<std::setprecision(12)<<"record,frame,tick,epoch,generation,measurement,width,height,logical_w,logical_h,view,bloom,strength_tenths,hud,lighting,ambient,light_strength,radius_multiplier,inspector,timing,preview_mask,pass_mask,timestamp_bits,query_status,received_frame,total_us";
    for (unsigned i=0;i<=ShadowInspect;++i) out_<<','<<passNames[i]<<"_us";
    out_<<legacyCpuColumns()
        <<",particle_emission_us,seed,scene,particle_script,particle_capacity,particle_rate,particle_steady"
        <<particleCounters<<",shader_gallery_us,gallery_effect,shader_version,map_width,map_height,map_script"
        <<mapCounters<<navCounters<<",runtime_revision,event_sequence,recorded_events,verified_events,state_hash,replay_mode\n";
}
void Metrics::prefix(const FrameInfo& f,const char* kind,QueryStatus query) {
    out_<<kind<<','<<f.id<<','<<f.tick<<','<<f.epoch<<','<<f.generation<<','<<f.measurement<<','
        <<f.pixels.width<<','<<f.pixels.height<<','<<f.logical.width<<','<<f.logical.height<<','
        <<name(f.display.view)<<','<<f.display.bloom<<','<<f.display.strength<<','<<f.display.hud<<','
        <<f.display.lighting<<','<<f.display.ambient<<','<<f.display.lightStrength<<','<<f.display.radiusMultiplier<<','
        <<f.inspector<<','<<f.timing<<','<<f.previews<<','<<f.mask<<',';
    if (f.timestampBits>=0) out_<<f.timestampBits;
    out_<<','<<(query==QueryStatus::Off ? (f.timing?"unavailable":"off") : status(query))<<',';
}
void Metrics::record(const CpuSample& c) {
    if (!out_.is_open()) return;
    prefix(c.info,"cpu",c.status);
    out_<<','<<c.total;
    const auto& s=c.stats;
    for (unsigned i=0;i<=ShadowInspect;++i) { out_<<','; if (s.mask&(1u<<i)) out_<<s.cpuUs[i]; }
    out_<<','<<c.update<<','<<c.prepare<<',';
    if (c.info.inspector || c.build>0) out_<<c.build;
    out_<<','; if (c.poll>0) out_<<c.poll;
    out_<<','<<s.passes<<','<<s.draws<<','<<s.worldSprites<<','<<s.bytes
        <<','<<s.guiVertices<<','<<s.guiIndices<<','<<s.vertexBytes<<','<<s.indexBytes<<','<<s.fontUpload
        <<','<<c.hdrBytes<<','<<c.previewBytes<<','<<s.fontBytes;
    for (unsigned i=0;i<=ShadowInspect;++i) out_<<','<<s.passDraws[i];
    for (unsigned i=0;i<=ShadowInspect;++i) out_<<','<<s.passBytes[i];
    out_<<',';
    if (c.info.display.lighting) out_<<c.lightExtract;
    out_<<','<<c.activeLights<<','<<c.droppedLights<<','<<c.lightSize.width<<','<<c.lightSize.height
        <<','<<s.lightUniformBytes<<','<<c.info.display.normalMapping
        <<','<<(c.info.display.normalMaterial.kind==NormalMaterialKind::Flat?"flat":"tile")
        <<','<<c.info.display.normalMaterial.strength<<','<<c.normalSize.width<<','<<c.normalSize.height
        <<','<<c.normalPrepare<<','<<c.materialBytes<<','<<c.materialUpload
        <<','<<c.info.display.shadows<<','<<c.info.display.shadowLights<<','<<(c.info.display.demoOccluders?"demo":"none")
        <<','<<s.activeCasters<<','<<s.skippedCasters<<','<<s.occluderCount<<','<<s.edgeCount
        <<','<<s.maskRebuilds<<','<<s.maskDraws<<','<<s.lightDraws<<','<<s.inspectRebuilds<<','<<s.inspectDraws
        <<','<<s.geometryUs<<','<<s.maskUs<<','<<s.lightSubmitUs<<','<<s.shadowVertexBytes
        <<','<<c.shadowBytes<<','<<s.fullscreen<<',';
    if (s.mask&(1u<<ParticleEmission)) out_<<s.cpuUs[ParticleEmission];
    runConfig(out_,c.info);
    if (c.info.particles) {
        out_<<','<<c.activeParticles<<','<<c.droppedParticles<<','<<c.activeFields
            <<','<<c.forceUpdateUs<<','<<c.snapshotPrepareUs<<','<<s.instanceUploadUs<<','<<s.instanceUploadBytes
            <<','<<s.activeInstances<<','<<s.particleDrawCalls;
    } else {
        out_<<",,,,,,,,,";
    }
    out_<<','<<c.validGpuSamples<<',';
    if (s.mask&(1u<<ShaderGalleryPass)) out_<<s.cpuUs[ShaderGalleryPass];
    galleryConfig(out_,c.info);
    mapConfig(out_,c.info);
    if (c.info.mapWidth) {
        const auto& m=c.map;
        out_<<',';
        if (m.generationUs || m.validationUs) out_<<m.generationUs;
        out_<<',';
        if (m.generationUs || m.validationUs) out_<<m.validationUs;
        out_<<','<<m.collisionUs<<','<<m.visiblePrepareUs<<','<<m.totalTiles<<','<<m.visibleTiles
            <<','<<s.submittedTiles<<','<<s.tileDrawCalls<<','<<s.tileVertexBytes
            <<','<<m.camera.x<<','<<m.camera.y<<','<<m.player.x<<','<<m.player.y<<','<<m.hitX<<','<<m.hitY;
    } else {
        static const auto emptyMap=blanks(mapCounters);
        out_<<emptyMap;
    }
    if (c.info.navigation) {
        const auto& n=c.nav;
        out_<<','<<n.requested<<','<<n.active<<','<<n.phase<<','<<n.hp<<','<<n.maxHP
            <<','<<n.playerSpeed<<','<<n.enemySpeed<<','<<n.guardTicks<<','<<n.offerSerial
            <<','<<n.choiceSerial<<','<<n.lastChoice<<','<<n.targetX<<','<<n.targetY
            <<','<<n.fieldRebuilds<<','<<n.cacheReuses<<','<<n.visited<<','<<n.expanded
            <<','<<n.graphEvents<<','<<n.fieldEvents<<',';
        if (n.graphEvents) out_<<n.graphUs;
        out_<<',';
        if (n.fieldEvents) out_<<n.fieldUs;
        out_<<','<<n.enemyUpdateUs<<','<<n.enemyCollisionUs<<','<<n.upgradeUs
            <<','<<s.submittedEnemies<<','<<s.enemyDrawCalls<<','<<s.enemyVertexBytes;
    } else {
        static const auto empty=blanks(navCounters);
        out_<<empty;
    }
    replayConfig(out_,c.info);
    out_<<'\n';
}
void Metrics::write(const GpuSample& g) {
    if (!out_.is_open()) return;
    prefix(g.info,"gpu",g.status); out_<<g.received;
    auto duration=[&](unsigned index) {
        out_<<',';
        if (g.status==QueryStatus::Complete && g.us[index]) out_<<*g.us[index];
    };
    for (unsigned i=0;i<=ShadowInspect+1;++i) duration(i);
    static const auto emptyLegacy=blanks(legacyCpuColumns());
    static const auto emptyParticle=blanks(particleCounters);
    out_<<emptyLegacy;
    duration(ParticleEmission+1);
    runConfig(out_,g.info);
    out_<<emptyParticle;
    duration(ShaderGalleryPass+1);
    galleryConfig(out_,g.info);
    mapConfig(out_,g.info);
    static const auto emptyMap=blanks(mapCounters);
    static const auto emptyNav=blanks(navCounters);
    out_<<emptyMap<<emptyNav;
    replayConfig(out_,g.info);
    out_<<'\n';
}
void Metrics::retired(GpuSample g) { if (out_.is_open()) retired_.push_back(std::move(g)); }
void Metrics::flushRetired() {
    for (const auto& result:retired_) write(result);
    retired_.clear();
}
void Metrics::close() {
    if (!out_.is_open()) return;
    flushRetired();
    out_.close();
}
Metrics::~Metrics() {
    try {close();} catch (const std::exception& e) {std::cerr<<"close metrics: "<<e.what()<<'\n';}
}
void report(const std::vector<CpuSample>& samples) {
    if (samples.empty()) return;
    auto metric=[&](const char* label,auto value) {
        std::vector<double> sorted;
        for (const auto& s:samples) if (auto v=value(s)) sorted.push_back(*v);
        if (sorted.empty()) return;
        std::sort(sorted.begin(),sorted.end());
        auto q=[&](double p){ return sorted[std::size_t(std::ceil(p*sorted.size()))-1]; };
        std::cout<<"metric="<<label<<" n="<<sorted.size()<<" p50="<<q(.5)<<" p95="<<q(.95)<<" p99="<<q(.99)<<'\n';
    };
    std::cout<<"warmup=120 samples="<<samples.size()<<" clock=steady units=us\n";
    metric("frame",[](const auto& c){ return std::optional<double>(c.total); });
    metric("fixed_update",[](const auto& c){return std::optional<double>(c.update);});
    metric("prepare",[](const auto& c){return std::optional<double>(c.prepare);});
    metric("light_extract",[](const auto& c)->std::optional<double>{
        if (c.info.display.lighting) return c.lightExtract;
        return {};
    });
    metric("draw_calls",[](const auto& c){return std::optional<double>(c.stats.draws);});
    metric("sprites",[](const auto& c){return std::optional<double>(c.stats.sprites);});
    metric("uploaded_bytes",[](const auto& c){return std::optional<double>(c.stats.bytes+c.stats.vertexBytes+c.stats.indexBytes+c.stats.fontUpload+c.stats.lightUniformBytes+c.stats.shadowVertexBytes+c.stats.instanceUploadBytes+c.materialUpload);});
    for (unsigned i=0;i<PassCount;++i) metric(passNames[i],[&](const auto& c)->std::optional<double>{
        if (c.stats.mask&(1u<<i)) return c.stats.cpuUs[i]; return {};
    });
}
}
