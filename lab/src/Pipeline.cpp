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
using Observer=std::function<void(const RenderTarget2D&)>;
RenderTarget2D attachment(const char* label,int w,int h,const Observer& observer,
                          RenderTarget2D::Format format=RenderTarget2D::Format::HDR16F) {
    RenderTarget2D target(label,w,h,format);
    if (observer) observer(target);
    return target;
}
Size thumbnailSize(Size s) {
    const double scale=std::min(1.,256./std::max(s.width,s.height));
    return {std::max(1,int(std::round(s.width*scale))),std::max(1,int(std::round(s.height*scale)))};
}
std::uint64_t hdrBytes(Size s,bool lighting) {
    // storageBytes checks each product before multiplication; the sum of seven
    // individually budget-limited attachments fits comfortably in uint64_t.
    return (lighting ? 4 : 1)*RenderTarget2D::storageBytes(s.width,s.height)
        + 3*RenderTarget2D::storageBytes(half(s.width),half(s.height));
}
std::uint64_t ldrBytes(Size s,unsigned count) {
    const Size thumb=thumbnailSize(s);
    return std::uint64_t(thumb.width)*thumb.height*4*count;
}
void checkBudget(Size s,bool lighting,bool previews,bool lightPreview) {
    if (hdrBytes(s,lighting)+ldrBytes(s,4u*previews+lightPreview)>RenderTarget2D::budget)
        throw std::length_error("Lab target group exceeds 256 MiB");
}
}
Pipeline::LightingTargets::LightingTargets(int w,int h,const Checkpoint& observe)
    : albedo(attachment("Albedo",w,h,observe)),emission(attachment("Emission",w,h,observe)),
      light(attachment("Light",w,h,observe)) {}
Pipeline::Targets::Targets(int w,int h,const Checkpoint& observe)
    : scene(attachment("SceneColor",w,h,observe)),bright(attachment("Bright",half(w),half(h),observe)),
      ping(attachment("Ping",half(w),half(h),observe)),pong(attachment("Pong",half(w),half(h),observe)) {}
void Pipeline::Targets::allocatePreviews(const Checkpoint& observe) {
    if (thumbnails) return;
    const Size s{scene.width(),scene.height()};
    checkBudget(s,bool(lighting),true,lighting && lighting->thumbnail);
    const Size thumb=thumbnailSize(s);
    auto target=[&](const char* label) {
        return attachment(label,thumb.width,thumb.height,observe,RenderTarget2D::Format::Display8);
    };
    thumbnails.reset(new Thumbnails{target("Scene preview"),target("Bright preview"),target("Blur preview"),target("Final preview")});
}
void Pipeline::Targets::allocateLightPreview(const Checkpoint& observe) {
    if (lighting->thumbnail) return;
    const Size s{scene.width(),scene.height()};
    checkBudget(s,true,bool(thumbnails),true);
    const Size thumb=thumbnailSize(s);
    lighting->thumbnail=std::make_unique<RenderTarget2D>(
        attachment("Light preview",thumb.width,thumb.height,observe,RenderTarget2D::Format::Display8));
}
std::uint64_t Pipeline::previewBytes() const {
    if (!targets_) return 0;
    unsigned count=targets_->thumbnails ? 4 : 0;
    if (targets_->lighting && targets_->lighting->thumbnail) ++count;
    return ldrBytes(size(),count);
}
std::array<PreviewView,4> Pipeline::previews() {
    triangle_.requireCurrent("Lab preview borrow");
    if (!drawable_) throw std::logic_error("cannot borrow suspended targets");
    auto& t=*targets_;
    t.allocatePreviews(allocationCheckpoint_);
    std::array<PreviewView,4> views;
    for (unsigned i=0;i<views.size();++i) {
        const auto& src=(i==1 || i==2)?t.bright:t.scene;
        const auto& thumb=(*t.thumbnails)[i];
        views[i]={thumb.colorName(),{src.width(),src.height()},{thumb.width(),thumb.height()},generation_};
    }
    return views;
}
PreviewView Pipeline::lightPreview() {
    triangle_.requireCurrent("Lab light preview borrow");
    if (!drawable_ || !lightingEnabled_) throw std::logic_error("Light preview requires enabled drawable lighting");
    auto& t=*targets_;
    t.allocateLightPreview(allocationCheckpoint_);
    const auto& thumb=*t.lighting->thumbnail;
    return {thumb.colorName(),size(),{thumb.width(),thumb.height()},generation_};
}
Pipeline::Pipeline() : bright_(program("bright.frag")), blur_(program("blur.frag")),
    display_(program("display.frag")),light_(program("light.frag")),compose_(program("compose.frag")),
    white_(ImageData{1,1,{255,255,255,255}}) {}
Pipeline::~Pipeline() = default;
Size Pipeline::size() const { return targets_ ? Size{targets_->scene.width(),targets_->scene.height()} : Size{}; }
Size Pipeline::lightSize() const { return targets_ && targets_->lighting ? size() : Size{}; }
std::uint64_t Pipeline::storage() const { return targets_ ? hdrBytes(size(),bool(targets_->lighting)) : 0; }
bool Pipeline::resize(Size s,bool lighting) {
    triangle_.requireCurrent("Lab resize");
    if (s.width<0 || s.height<0) throw std::invalid_argument("negative framebuffer size");
    drawable_=false;
    if (!s.width || !s.height) return false;
    if (s.width!=size().width || s.height!=size().height) {
        const bool previews=targets_ && targets_->thumbnails;
        const bool lightPreview=lighting && targets_ && targets_->lighting && targets_->lighting->thumbnail;
        checkBudget(s,lighting,previews,lightPreview);
        auto fresh=std::make_unique<Targets>(s.width,s.height,allocationCheckpoint_);
        if (lighting) fresh->lighting=std::make_unique<LightingTargets>(s.width,s.height,allocationCheckpoint_);
        if (previews) fresh->allocatePreviews(allocationCheckpoint_);
        if (lightPreview) fresh->allocateLightPreview(allocationCheckpoint_);
        targets_.swap(fresh);
        ++generation_;
    } else if (lighting && !targets_->lighting) {
        checkBudget(s,true,bool(targets_->thumbnails),false);
        targets_->lighting=std::make_unique<LightingTargets>(s.width,s.height,allocationCheckpoint_);
    }
    lightingEnabled_=lighting;
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
    if (d.lighting!=lightingEnabled_) throw std::logic_error("lighting changed after target preparation");
    if ((tools.previews & 15u) && !t.thumbnails) throw std::logic_error("preview targets missing");
    if (previewNeeded(View::Light) && (!d.lighting || !t.lighting->thumbnail))
        throw std::logic_error("light preview target missing");
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
        if (!d.lighting) pass(World,[&] {
            t.scene.bindDraw();
            constexpr GLfloat background[]={.002f,.004f,.008f,1};
            glClearBufferfv(GL_COLOR,0,background);
            sprites(picture.world,picture.worldCamera,s);
            s.worldSprites=s.sprites;
        });
        else {
            auto& lit=*t.lighting;
            pass(Albedo,[&] {
                lit.albedo.bindDraw();
                constexpr GLfloat background[]={.002f,.004f,.008f,1};
                glClearBufferfv(GL_COLOR,0,background);
                sprites(picture.albedo,picture.worldCamera,s);
            });
            pass(Emission,[&] {
                lit.emission.bindDraw();
                constexpr GLfloat black[]={0,0,0,1};
                glClearBufferfv(GL_COLOR,0,black);
                sprites(picture.emission,picture.worldCamera,s);
                s.worldSprites=s.sprites;
            });
            pass(Light,[&] {
                lit.light.bindDraw();
                light_.bind();
                const auto projection=picture.worldCamera.viewProjectionMatrix();
                const auto& m=projection.values;
                light_.setVec4("projection",m[0],m[5],m[12],m[13]);
                light_.setVec2("framebufferSize",float(t.scene.width()),float(t.scene.height()));
                light_.setFloat("ambient",d.ambient);
                static_assert(LightList::capacity==32,"match the GLSL uniform capacity");
                const int count=int(picture.lights.size());
                light_.setInt("lightCount",count);
                if (count) {
                    std::array<float,4*LightList::capacity> centers, colors;
                    unsigned i=0;
                    for (const auto& light:picture.lights) {
                        centers[i]=light.position.x; centers[i+1]=light.position.y;
                        centers[i+2]=1.f/light.radius; centers[i+3]=0;
                        colors[i]=light.color.r*light.intensity; colors[i+1]=light.color.g*light.intensity;
                        colors[i+2]=light.color.b*light.intensity; colors[i+3]=0;
                        i+=4;
                    }
                    light_.setVec4Array("centerInvRadius[0]",count,centers.data());
                    light_.setVec4Array("radiance[0]",count,colors.data());
                    s.lightUniformBytes=2*i*sizeof(float);
                }
                glBindVertexArray(triangle_.id());
                triangle();
            });
            pass(SceneCompose,[&] {
                t.scene.bindDraw();
                compose_.bind();
                lit.albedo.bindColor(0); compose_.setInt("albedo",0);
                lit.light.bindColor(1); compose_.setInt("illumination",1);
                lit.emission.bindColor(2); compose_.setInt("emission",2);
                glBindVertexArray(triangle_.id());
                triangle();
            });
        }
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
            const auto& source=view==View::Bright ? t.bright : view==View::Blur ? t.pong :
                               view==View::Light ? t.lighting->light : t.scene;
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
            if (previewNeeded(View::Light)) {
                t.lighting->thumbnail->bindDraw();
                displayView(View::Light);
                s.previewMask |= 1u<<unsigned(View::Light);
            }
        });
        pass(Output,[&] {
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER,0);
            glViewport(0,0,t.scene.width(),t.scene.height());
            displayView(d.view);
        });
        if ((d.view==View::Scene || d.view==View::Final || d.view==View::Light) && !picture.debug.empty())
            pass(Debug,[&]{ sprites(picture.debug,picture.worldCamera,s); });
        if (d.hud) pass(Overlay,[&]{ sprites(picture.hud,picture.hudCamera,s); });
        if (tools.gui) pass(Gui,[&]{tools.gui(s);});
    }
    detail::checkGL("Lab frame state restoration");
    s.complete=true;
}
}
