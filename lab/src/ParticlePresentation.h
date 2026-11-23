#pragma once
#include "ParticleSimulation.h"
#include "Inspector.h"

namespace lab {
struct ParticleEditor {
    // -1 selects the emitter; 0..7 index the bounded force-field array.
    int selected=0;
    bool guides=true, dragEnabled=true, dragging=false;
    tiny2d::Vector2 dragOffset{};
    ParticleTickInput pending;
    std::string notice;
    void pointer(const RoutedInput&,const ParticleSimulation&,const tiny2d::Camera2D&,tiny2d::Size,View);
    ParticleTickInput takeTick(const ParticleSimulation&,Motion);
};

const char* fieldName(FieldKind);
tiny2d::Camera2D particleCamera(tiny2d::Size logical);
ParticleTickInput particleDemoTick(const ParticleSimulation&);
void prepareParticles(Picture&,const ParticleSimulation&,const ParticleEditor&,const Options&,bool paused,tiny2d::Size logical);
void particlePanel(ToolState&,ParticleEditor&,Commands&,const ParticleSimulation&,const Options&,bool paused,
                   const History&,const std::string& timestamp);
}
