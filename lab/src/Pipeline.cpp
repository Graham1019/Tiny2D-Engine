#include "Pipeline.h"
#include "FrameState.h"
#include "ShaderAssets.h"
#include <stdexcept>

namespace lab {
using namespace tiny2d;
namespace {
int half(int extent) { return extent/2 + extent%2; }
ShaderProgram program(const char* fragment) {
    return {readShaderAsset("fullscreen.vert"),readShaderAsset(fragment),"fullscreen.vert",fragment};
}
}
Pipeline::Targets::Targets(int w,int h,std::uint64_t total) : scene("SceneColor",w,h),
        bright("Bright",half(w),half(h)),ping("Ping",half(w),half(h)),
        pong("Pong",half(w),half(h)),bytes(total) {}
Pipeline::Pipeline() : bright_(program("bright.frag")), blur_(program("blur.frag")),
    display_(program("display.frag")),white_(ImageData{1,1,{255,255,255,255}}) {}
Pipeline::~Pipeline() = default;
Size Pipeline::size() const { return targets_ ? Size{targets_->scene.width(),targets_->scene.height()} : Size{}; }
std::uint64_t Pipeline::storage() const { return targets_ ? targets_->bytes : 0; }
bool Pipeline::resize(Size s) {
    triangle_.requireCurrent("Lab resize");
    if (s.width<0 || s.height<0) throw std::invalid_argument("negative framebuffer size");
    drawable_ = false;
    if (!s.width || !s.height) return false;
    if (s.width!=size().width || s.height!=size().height) {
        const auto bytes=RenderTarget2D::storageBytes(s.width,s.height)
            + 3*RenderTarget2D::storageBytes(half(s.width),half(s.height));
        if (bytes>RenderTarget2D::budget) throw std::length_error("Lab target group exceeds 256 MiB");
        auto fresh=std::make_unique<Targets>(s.width,s.height,bytes);
        targets_.swap(fresh);
    }
    drawable_=true;
    return true;
}
void Pipeline::sprites(const std::vector<Quad>& quads,const Camera2D& camera,Stats& s) {
    batch_.beginFrame(camera);
    auto record=[&] {
        const auto& stats=batch_.stats();
        s.draws+=stats.drawCalls;
        s.sprites+=stats.submittedSprites;
        s.bytes+=stats.uploadedVertexBytes;
    };
    try {
        for (const auto& q:quads) batch_.draw(white_,q.position,q.size,q.angle,q.color);
        batch_.endFrame();
    } catch (...) {
        record();
        try { batch_.cancelFrame(); } catch (...) {}
        throw;
    }
    record();
}

void Pipeline::draw(const Picture& picture,const Display& d,Stats& s) {
    s={};
    triangle_.requireCurrent("Lab draw");
    d.validate();
    if (!drawable_) throw std::logic_error("Lab draw requires a matching drawable target group");
    auto& t=*targets_;
    const bool mix=d.view==View::Final && d.bloom && d.strength>0;
    const bool blur=mix || d.view==View::Blur;
    auto pass=[&](Pass index,auto submit) {
        const auto start=Clock::now();
        submit();
        detail::checkGL("Lab pass submission");
        ++s.passes;
        s.cpuUs[index]=micros(start);
    };
    auto fullscreen=[&](ShaderProgram& shader,const RenderTarget2D& source) {
        shader.bind();
        source.bindColor(0);
        shader.setInt("source",0);
        glBindVertexArray(triangle_.id());
    };
    auto triangle=[&] {
        glDrawArrays(GL_TRIANGLES,0,3);
        detail::checkGL("Lab fullscreen triangle");
        ++s.draws; ++s.fullscreen;
    };
    {
        FrameState state;
        pass(World,[&] {
            t.scene.bindDraw();
            constexpr GLfloat background[]={.002f,.004f,.008f,1};
            glClearBufferfv(GL_COLOR,0,background);
            sprites(picture.world,picture.worldCamera,s);
        });
        if (blur || d.view==View::Bright) pass(Extract,[&] {
            t.bright.bindDraw();
            fullscreen(bright_,t.scene);
            triangle();
        });
        if (blur) {
            auto filter=[&](RenderTarget2D& dst,const RenderTarget2D& src,float x,float y) {
                dst.bindDraw();
                fullscreen(blur_,src);
                blur_.setVec2("axis",x,y);
                triangle();
            };
            pass(Horizontal,[&]{ filter(t.ping,t.bright,1,0); });
            pass(Vertical,[&]{ filter(t.pong,t.ping,0,1); });
        }
        pass(Output,[&] {
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER,0);
            glViewport(0,0,t.scene.width(),t.scene.height());
            const auto& source=d.view==View::Bright ? t.bright : d.view==View::Blur ? t.pong : t.scene;
            fullscreen(display_,source);
            // Even disabled samplers point at a complete, initialized image.
            (mix ? t.pong : source).bindColor(1);
            display_.setInt("bloom",1);
            display_.setFloat("strength",mix ? d.strength/10.f : 0.f);
            triangle();
        });
        if (d.hud) pass(Overlay,[&]{ sprites(picture.hud,picture.hudCamera,s); });
    }
    detail::checkGL("Lab frame state restoration");
    s.complete=true;
}
}
