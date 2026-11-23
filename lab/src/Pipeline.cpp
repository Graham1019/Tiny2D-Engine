#include "Pipeline.h"
#include "FrameState.h"
#include "ShaderAssets.h"
#include <stdexcept>
#include <cmath>
#include <algorithm>

namespace lab {
using namespace tiny2d;
namespace {
int half(int extent) { return extent/2 + extent%2; }
ShaderProgram program(const char* fragment) {
    return {readShaderAsset("fullscreen.vert"),readShaderAsset(fragment),"fullscreen.vert",fragment};
}
}
namespace {
RenderTarget2D hdr(const char* label,int w,int h,const std::function<void(const RenderTarget2D&)>& observer) {
    RenderTarget2D target(label,w,h);
    if (observer) observer(target);
    return target;
}
}
Pipeline::Targets::Targets(int w,int h,std::uint64_t total,const Checkpoint& observe)
    : scene(hdr("SceneColor",w,h,observe)), bright(hdr("Bright",half(w),half(h),observe)),
      ping(hdr("Ping",half(w),half(h),observe)),pong(hdr("Pong",half(w),half(h),observe)),bytes(total) {}
void Pipeline::Targets::allocatePreviews() {
    if (thumbnails) return;
    const double scale=std::min(1.,256./std::max(scene.width(),scene.height()));
    const int w=std::max(1,int(std::round(scene.width()*scale)));
    const int h=std::max(1,int(std::round(scene.height()*scale)));
    auto target=[&](const char* label){return RenderTarget2D(label,w,h,RenderTarget2D::Format::Display8);};
    thumbnails.reset(new Thumbnails{target("Scene preview"),target("Bright preview"),target("Blur preview"),target("Final preview")});
}
std::uint64_t Pipeline::previewBytes() const {
    if (!targets_ || !targets_->thumbnails) return 0;
    const auto& thumb=targets_->thumbnails->front();
    return 4ull*4*thumb.width()*thumb.height();
}
std::array<PreviewView,4> Pipeline::previews() {
    triangle_.requireCurrent("Lab preview borrow");
    if (!drawable_) throw std::logic_error("cannot borrow suspended targets");
    auto& t=*targets_;
    t.allocatePreviews();
    std::array<PreviewView,4> views;
    for (unsigned i=0;i<views.size();++i) {
        const auto& src=(i==1 || i==2)?t.bright:t.scene;
        const auto& thumb=(*t.thumbnails)[i];
        views[i]={thumb.colorName(),{src.width(),src.height()},{thumb.width(),thumb.height()},generation_};
    }
    return views;
}
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
        auto fresh=std::make_unique<Targets>(s.width,s.height,bytes,allocationCheckpoint_);
        if (targets_ && targets_->thumbnails) fresh->allocatePreviews();
        targets_.swap(fresh);
        ++generation_;
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

void Pipeline::draw(const Picture& picture,const Display& d,Stats& s,const DrawTools& tools) {
    s={};
    triangle_.requireCurrent("Lab draw");
    d.validate();
    if (!drawable_) throw std::logic_error("Lab draw requires a matching drawable target group");
    auto& t=*targets_;
    const bool mix=d.view==View::Final && d.bloom && d.strength>0;
    auto previewNeeded=[&](View v){return (tools.previews & (1u<<unsigned(v)))!=0;};
    const bool finalPreview=previewNeeded(View::Final);
    const bool blur=mix || d.view==View::Blur || previewNeeded(View::Blur) || (finalPreview && d.bloom && d.strength>0);
    if (tools.previews && !t.thumbnails) throw std::logic_error("preview targets missing");
    auto pass=[&](Pass index,auto submit) {
        const auto start=Clock::now();
        const auto before=s.draws, bytesBefore=s.bytes;
        if (tools.timer) tools.timer->mark(1+index,false);
        submit();
        if (tools.timer) tools.timer->mark(1+index,true);
        detail::checkGL("Lab pass submission");
        s.passes+=index==Preview ? unsigned(s.draws-before) : 1;
        s.passBytes[index]=s.bytes-bytesBefore;
        s.mask |= 1u<<index;
        s.passDraws[index]=unsigned(s.draws-before);
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
            s.worldSprites=s.sprites;
        });
        if (blur || d.view==View::Bright || previewNeeded(View::Bright)) pass(Extract,[&] {
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
        auto displayView=[&](View view) {
            const auto& source=view==View::Bright ? t.bright : view==View::Blur ? t.pong : t.scene;
            const bool add=view==View::Final && d.bloom && d.strength>0;
            fullscreen(display_,source);
            (add ? t.pong : source).bindColor(1);
            display_.setInt("bloom",1);
            display_.setFloat("strength",add ? d.strength/10.f : 0.f);
            triangle();
        };
        if (tools.previews) pass(Preview,[&] {
            for (unsigned i=0;i<4;++i) if (tools.previews & (1u<<i)) {
                (*t.thumbnails)[i].bindDraw();
                displayView(static_cast<View>(i));
                s.previewMask |= 1u<<i;
            }
        });
        pass(Output,[&] {
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER,0);
            glViewport(0,0,t.scene.width(),t.scene.height());
            displayView(d.view);
        });
        if ((d.view==View::Scene || d.view==View::Final) && !picture.debug.empty())
            pass(Debug,[&]{ sprites(picture.debug,picture.worldCamera,s); });
        if (d.hud) pass(Overlay,[&]{ sprites(picture.hud,picture.hudCamera,s); });
        if (tools.gui) pass(Gui,[&]{tools.gui(s);});
    }
    detail::checkGL("Lab frame state restoration");
    s.complete=true;
}
}
