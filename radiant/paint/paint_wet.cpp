// Curtis wet simulation, derived from procart.watercolor and Washes (MIT; WASHES_LICENSE.txt).
// The caller owns all planes and scratch storage (PE9, D5.5.2).
#include "paint_wet_internal.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace {
enum Plane { PAPER, WET, WET_TMP, PRESSURE, U, V, U_NEW, V_NEW, G };
template<typename T, typename Sheet> struct Wet {
    Sheet& s; const PaintWetParams& p; int w, h; size_t n;
    int deposited() const { return paint_wet_deposited(s.pigments); }
    int scratch() const { return paint_wet_scratch(s.pigments); }
    int binary_plane() const { return 8 + 3 * s.pigments; }
    const float* adsorption() const { return p.adsorption_table ? p.adsorption_table : p.adsorption; }
    T* plane(int k) { return s.cells + n * k; }
    void copy(int to, int from, int x0, int x1, int y0, int y1) {
        T* a = plane(to); const T* b = plane(from);
        for (int y = y0; y < y1; ++y) for (int x = x0; x < x1; ++x) a[y*w+x] = b[y*w+x];
    }
    // Running sums reproduce the reference's clamped separable box blur.
    void blur(int to, int r, PaintRect a) {
        T* src=plane(binary_plane()), *tmp=plane(binary_plane()+3), *dst=plane(to);
        T inv=1.0f/(2*r+1);
        int lo=std::max(0,a.x0-r), hi=std::min(w-1,a.x0+r);
        int left=std::max(0,r-a.x0), right=std::max(0,a.x0+r-(w-1));
        for (int y=std::max(0,a.y0-r); y<std::min(h,a.y1+r); ++y) {
            int row=y*w; T value=(left*src[row]+right*src[row+w-1])*inv;
            for (int x=lo;x<=hi;++x) value+=src[row+x]*inv;
            tmp[row+a.x0]=value;
            for (int x=a.x0+1;x<a.x1;++x) {
                value+=(src[row+std::min(w-1,x+r)]-src[row+std::max(0,x-r-1)])*inv;
                tmp[row+x]=value;
            }
        }
        int top=std::max(0,a.y0-r), bottom=std::min(h-1,a.y0+r);
        int above=std::max(0,r-a.y0), below=std::max(0,a.y0+r-(h-1));
        // Advance all columns together: each row then reads and writes contiguous samples.
        for (int x=a.x0;x<a.x1;++x)
            dst[a.y0*w+x]=(above*tmp[x]+below*tmp[(h-1)*w+x])*inv;
        for (int y=top;y<=bottom;++y)
            for (int x=a.x0;x<a.x1;++x) dst[a.y0*w+x]+=tmp[y*w+x]*inv;
        for (int y=a.y0+1;y<a.y1;++y) {
            int high=std::min(h-1,y+r)*w, low=std::max(0,y-r-1)*w;
            for (int x=a.x0;x<a.x1;++x)
                dst[y*w+x]=dst[(y-1)*w+x]+(tmp[high+x]-tmp[low+x])*inv;
        }
    }
    // The same evaporation stage also supports a direct dry without advancing flow.
    void evaporate(T evaporation) {
        T *wet=plane(WET), *u=plane(U), *v=plane(V);
        s.wet_cells=0;for(size_t i=0;i<n;++i){
            bool pigment=false;
            for (int k=0;k<s.pigments && !pigment;++k) pigment=plane(G+k)[i]!=0;
            if(wet[i]==0 && !pigment && u[i]==0 && v[i]==0) continue;
            T next=wet[i]*evaporation;
            if(next<0.025f){for(int k=0;k<s.pigments;++k){T* g=plane(G+k),*d=plane(deposited()+k);d[i]=std::min(T(1),d[i]+g[i]);g[i]=0;}wet[i]=u[i]=v[i]=0;}
            else {wet[i]=next;if(next>0.04f)++s.wet_cells;}
        }
    }
    void run() {
        PaintRect a=s.active;
        T *wet=plane(WET), *pr=plane(PRESSURE), *u=plane(U), *v=plane(V);
        if (a.x1>a.x0 && a.y1>a.y0) {
            int x0=std::max(1,a.x0),x1=std::min(w-1,a.x1),y0=std::max(1,a.y0),y1=std::min(h-1,a.y1);
            int px0=std::max(0,a.x0-1),px1=std::min(w,a.x1+1),py0=std::max(0,a.y0-1),py1=std::min(h,a.y1+1);
            copy(WET_TMP,WET,px0,px1,py0,py1);
            T* t=plane(WET_TMP);
            for(int y=y0;y<y1;++y) for(int x=x0;x<x1;++x) {
                int i=y*w+x; T m=t[i],l=t[i-1],r=t[i+1],up=t[i-w],down=t[i+w];
                if(m>=1e-6f || l>=1e-6f || r>=1e-6f || up>=1e-6f || down>=1e-6f)
                    wet[i]=m+p.wet_diffusion*(l+r+up+down-4*m);
            }
            if(p.edges) {
                int pad=p.edge_kernel_large; T* binary=plane(binary_plane());
                for(int y=std::max(0,a.y0-pad);y<std::min(h,a.y1+pad);++y)
                    for(int x=std::max(0,a.x0-pad);x<std::min(w,a.x1+pad);++x) binary[y*w+x]=wet[y*w+x]>0.04f?1:0;
                blur(binary_plane()+1,p.edge_kernel,a); blur(binary_plane()+2,p.edge_kernel_large,a);
                T *near=plane(binary_plane()+1),*large=plane(binary_plane()+2);
                for(int y=a.y0;y<a.y1;++y) for(int x=a.x0;x<a.x1;++x) {
                    int i=y*w+x; T m=wet[i], deficit=1-near[i];
                    if(m>0.1f && deficit>0) pr[i]-=0.045f*deficit*large[i]*m*(m>=0.4f?1.0f:(m-0.1f)/0.3f);
                }
            }
            T *paper=plane(PAPER),*un=plane(U_NEW),*vn=plane(V_NEW);
            for(int y=y0;y<y1;++y) for(int x=x0;x<x1;++x) {
                int i=y*w+x;
                if(wet[i]<0.04f) {un[i]=vn[i]=0;continue;}
                T ui=u[i],vi=v[i];
                T lu=u[i-1]+u[i+1]+u[i-w]+u[i+w]-4*ui;
                T lv=v[i-1]+v[i+1]+v[i-w]+v[i+w]-4*vi;
                T nu=ui+0.42f*(-0.5f*(pr[i+1]-pr[i-1])-0.06f*(paper[i+1]-paper[i-1])+0.1f*lu-0.014f*ui);
                T nv=vi+0.42f*(-0.5f*(pr[i+w]-pr[i-w])-0.06f*(paper[i+w]-paper[i-w])+0.1f*lv-0.014f*vi)+p.tilt*0.08f*p.velocity_clamp;
                T speed=std::sqrt(nu*nu+nv*nv);
                if(speed>p.velocity_clamp){nu*=p.velocity_clamp/speed;nv*=p.velocity_clamp/speed;}
                un[i]=nu;vn[i]=nv;
            }
            copy(U,U_NEW,x0,x1,y0,y1);copy(V,V_NEW,x0,x1,y0,y1);
            for(int y=y0;y<y1;++y) for(int x=x0;x<x1;++x) pr[y*w+x]*=0.94f;
            for(int k=0;k<s.pigments;++k) copy(scratch()+k,G+k,px0,px1,py0,py1);
            for(int y=y0;y<y1;++y) for(int x=x0;x<x1;++x) {
                int i=y*w+x; if(wet[i]<0.04f) continue;
                T area=std::exp(-0.5f*((u[i+1]-u[i-1])+(v[i+w]-v[i-w]))*0.294f);
                T du=u[i]*0.294f,dv=v[i]*0.294f,ax=std::fabs(du),ay=std::fabs(dv);
                T wl=0.5f*(du+ax),wr=0.5f*(ax-du),wm=1-ax;
                T wu=0.5f*(dv+ay)*area,wd=0.5f*(ay-dv)*area,wn=(1-ay)*area;
                for(int k=0;k<s.pigments;++k){T* g=plane(G+k);T* out=plane(scratch()+k);
                    T moved=wu*(wl*g[i-w-1]+wm*g[i-w]+wr*g[i-w+1])+wn*(wl*g[i-1]+wm*g[i]+wr*g[i+1])+wd*(wl*g[i+w-1]+wm*g[i+w]+wr*g[i+w+1]);
                    out[i]=std::min(T(1),moved);
                }
            }
            for(int k=0;k<s.pigments;++k) {
                T *g=plane(G+k),*d=plane(deposited()+k),*t2=plane(scratch()+k);
                for(int y=py0;y<py1;++y) for(int x=px0;x<px1;++x) {
                    int i=y*w+x;T m=t2[i];
                    g[i]=(y>=y0 && y<y1 && x>=x0 && x<x1 && wet[i]>=0.04f)?m+p.pigment_diffusion*(t2[i-1]+t2[i+1]+t2[i-w]+t2[i+w]-4*m):m;
                }
                const float* ads=adsorption()+k*3; T density=ads[0],staining=ads[1],gran=ads[2];
                for(int y=a.y0;y<a.y1;++y) for(int x=a.x0;x<a.x1;++x) {
                    int i=y*w+x;if(wet[i]<0.04f)continue;
                    T gi=g[i],di=d[i],down=gi*(1-paper[i]*gran)*density,up=di*(1+(paper[i]-1)*gran)*density/staining;
                    if(di+down>1)down=std::max(T(0),T(1)-di);
                    if(gi+up>1)up=std::max(T(0),T(1)-gi);
                    d[i]=di+down-up;g[i]=gi+up-down;
                }
            }
        }
        if(p.drying) evaporate((T)p.evaporation);
        if(++s.since_shrink>=60 && a.x1>a.x0 && a.y1>a.y0){
            s.since_shrink=0;int lx=a.x1,hx=a.x0-1,ly=a.y1,hy=a.y0-1;
            for(int y=a.y0;y<a.y1;++y) for(int x=a.x0;x<a.x1;++x){int i=y*w+x;
                bool pigment=false;
                for (int k=0;k<s.pigments && !pigment;++k) pigment=plane(G+k)[i]>0.001f;
                if(wet[i]>1e-6f || pigment || (x>0&&x<w-1&&y>0&&y<h-1&&pr[i]>0.001f)){
                    lx=std::min(lx,x);hx=std::max(hx,x);ly=std::min(ly,y);hy=std::max(hy,y);
                }
            }
            s.active=hx<lx?PaintRect{0,0,0,0}:PaintRect{std::max(0,lx-24),std::max(0,ly-24),std::min(w,hx+25),std::min(h,hy+25)};
        }
    }
};
}

template<typename T, typename Sheet>
static PaintResult wet_step(Sheet* s,const PaintWetParams* p,int32_t steps){
    PaintResult bad{PAINT_ERROR_ARGUMENT,"invalid wet sheet or parameters",{0,0,0,0},0};
    if(!s||!p||!s->cells||s->width<8||s->height<8||steps<0||s->since_shrink<0||s->since_shrink>INT32_MAX-steps||s->wet_cells<0) return bad;
    int w=s->width,h=s->height; PaintRect a=s->active;
    int32_t planes = paint_wet_layout(s->pigments);
    if (!planes || (s->pigments>3 && !p->adsorption_table)) return bad;
    // Check the complete planar span before forming any storage pointer (PE11).
    if ((uint64_t)w * (uint64_t)h > SIZE_MAX / (sizeof(T) * (size_t)planes)) return bad;
    if((int64_t)w*h>std::numeric_limits<int32_t>::max() || a.x0<0||a.y0<0||a.x1<a.x0||a.y1<a.y0||a.x1>w||a.y1>h) return bad;
    if(!std::isfinite(p->wet_diffusion)||p->wet_diffusion<0||p->wet_diffusion>0.2f||!std::isfinite(p->pigment_diffusion)||p->pigment_diffusion<0||p->pigment_diffusion>0.2f||p->edge_kernel<1||p->edge_kernel_large<p->edge_kernel||p->edge_kernel_large>(INT32_MAX-std::max(w,h)-1)/2||!std::isfinite(p->velocity_clamp)||p->velocity_clamp<=0||p->velocity_clamp>1.5f||!std::isfinite(p->tilt)||!std::isfinite(p->evaporation)||p->evaporation<0||p->evaporation>1) return bad;
    const float* ads=p->adsorption_table ? p->adsorption_table : p->adsorption;
    for(int k=0;k<s->pigments;++k) if(!std::isfinite(ads[3*k])||ads[3*k]<0||!std::isfinite(ads[3*k+1])||ads[3*k+1]<=0||!std::isfinite(ads[3*k+2])||ads[3*k+2]<0||ads[3*k+2]>1) return bad;
    Wet<T, Sheet> wet{*s,*p,w,h,(size_t)w*h};for(int i=0;i<steps;++i)wet.run();
    return PaintResult{PAINT_OK,nullptr,p->drying && steps ? PaintRect{0,0,w,h} : a,(uint64_t)w*h*(uint64_t)steps};
}

PaintResult paint_wet_step(PaintWetSheet* s, const PaintWetParams* p, int32_t steps) { return wet_step<float>(s, p, steps); }
PaintResult paint_wet_step64(PaintWetSheet64* s, const PaintWetParams* p, int32_t steps) { return wet_step<double>(s, p, steps); }

template<typename T> static PaintResult wet_dry(PaintWetSheetT<T>* s) {
    if (!paint_wet_sheet_valid(s)) return paint_fail(PAINT_ERROR_ARGUMENT, "invalid wet sheet");
    PaintWetParams p = {}; Wet<T, PaintWetSheetT<T>> wet{*s,p,s->width,s->height,(size_t)s->width*s->height};
    wet.evaporate((T)0);
    return paint_ok({0,0,s->width,s->height}, (uint64_t)s->width*s->height);
}
PaintResult paint_wet_dry(PaintWetSheet* s) { return wet_dry(s); }
PaintResult paint_wet_dry64(PaintWetSheet64* s) { return wet_dry(s); }
