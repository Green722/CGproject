/*
 * COMP4033 Final Project
 * 6 effects: Phong specular (#5), visible lights (#7), animated ball (#2),
 *            transparent glass (#4), particle fire (#3), shadow mapping (#6)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define GLEW_STATIC
#include <GL/glew.h>
#include <GL/glut.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ---------------------------------------------------------------------------
// Window / camera
// ---------------------------------------------------------------------------
int windowWidth = 1024, windowHeight = 768;
glm::vec3 cameraPos(-8.0f, 2.5f, 0.0f);
float     cameraYaw = -90.0f, cameraPitch = 0.0f;
bool      mouseDown = false;
int       mouseOldX, mouseOldY;
float     moveSpeed = 6.0f;
int       lastTime  = 0;
bool      keys[256] = {};
glm::mat4 currentView;

// ---------------------------------------------------------------------------
// Vertex formats
// ---------------------------------------------------------------------------
struct Vertex {        // scene geometry
    float x,y,z, nx,ny,nz, u,v;
};
struct ColorVertex {   // actor + ball
    float x,y,z, nx,ny,nz, r,g,b;
};
struct ParticleVert {  // fire particles
    float x,y,z, size, r,g,b,a;
};

// ---------------------------------------------------------------------------
// Shader source strings
// ---------------------------------------------------------------------------

// ---- Room shader (Phong + texture + shadow) ----
static const char* roomVS = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;
out vec3 fPos;
out vec3 fNorm;
out vec2 fUV;
out vec4 fPosLS;
uniform mat4 proj, view, model, lightSpaceMat;
void main(){
    vec4 wp = model * vec4(aPos,1);
    fPos  = wp.xyz;
    fNorm = normalize(mat3(transpose(inverse(model))) * aNormal);
    fUV   = aUV;
    fPosLS = lightSpaceMat * wp;
    gl_Position = proj * view * wp;
}
)";

static const char* roomFS = R"(
#version 330 core
in vec3 fPos; in vec3 fNorm; in vec2 fUV; in vec4 fPosLS;
out vec4 fragColor;
uniform sampler2D tex;
uniform sampler2D shadowMap;
uniform vec3 lightPos[3];
uniform vec3 lightColor[3];
uniform vec3 viewPos;
uniform bool useShadow;
uniform float sceneTime;

// Shadow mapping (#6) - fixed: explicit vec2 cast for textureSize
float shadowFactor(vec4 ls, vec3 n, vec3 ld){
    vec3 p = ls.xyz / ls.w * 0.5 + 0.5;
    if(p.z > 1.0) return 0.0;
    float bias = max(0.005*(1.0-dot(n,ld)), 0.001);
    float shadow = 0.0;
    vec2 ts = vec2(1.0) / vec2(textureSize(shadowMap,0));
    for(int x=-1;x<=1;x++) for(int y=-1;y<=1;y++){
        float d = texture(shadowMap, p.xy+vec2(x,y)*ts).r;
        shadow += (p.z - bias > d) ? 1.0 : 0.0;
    }
    return shadow / 9.0;
}

void main(){
    vec3 n = normalize(fNorm);

    // Dynamic texture (#1): scroll floor UV with time
    vec2 uv = fUV;
    if(n.y > 0.7) uv += vec2(sceneTime*0.012, sceneTime*0.007);
    vec3 tc = texture(tex, uv).rgb;

    // Bump mapping (#8): procedural normal perturbation on walls
    if(abs(n.y) < 0.3){
        vec3 dp1 = dFdx(fPos);
        vec3 dp2 = dFdy(fPos);
        vec2 duv1 = dFdx(fUV);
        vec2 duv2 = dFdy(fUV);
        float det = duv1.x*duv2.y - duv2.x*duv1.y;
        if(abs(det) > 0.0001){
            float s = 1.0/det;
            vec3 T = normalize(s*(duv2.y*dp1 - duv1.y*dp2));
            vec3 B = normalize(s*(-duv2.x*dp1 + duv1.x*dp2));
            float dhdu = cos(fUV.x*28.0)*sin(fUV.y*28.0)*28.0;
            float dhdv = sin(fUV.x*28.0)*cos(fUV.y*28.0)*28.0;
            n = normalize(n + 0.035*(dhdu*T + dhdv*B));
        }
    }

    // Dynamic light intensity: warm lights pulse (#1 extension)
    vec3 lc0 = lightColor[0]*(0.90+0.10*sin(sceneTime*1.8));
    vec3 lc1 = lightColor[1];
    vec3 lc2 = lightColor[2]*(0.90+0.10*sin(sceneTime*2.1+1.0));

    vec3 result = vec3(0);
    for(int i=0;i<3;i++){
        vec3 lc = (i==0)?lc0:(i==1)?lc1:lc2;
        vec3 ld = normalize(lightPos[i]-fPos);
        float d  = length(lightPos[i]-fPos);
        float att= 1.0/(1.0+0.07*d+0.017*d*d);
        vec3 amb = 0.12*lc*tc;
        float df = max(dot(n,ld),0.0);
        vec3 dif = df*lc*tc;
        vec3 vd  = normalize(viewPos-fPos);
        vec3 hd  = normalize(ld+vd);
        float sp = pow(max(dot(n,hd),0.0),64.0);
        vec3 spec= sp*lc*0.35;
        float sh = (i==0 && useShadow) ? shadowFactor(fPosLS,n,ld)*0.7 : 0.0;
        result  += amb + att*(1.0-sh)*(dif+spec);
    }
    fragColor = vec4(result,1);
}
)";

// ---- Actor/ball shader (Phong + vertex color) ----
static const char* actorVS = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec3 aColor;
out vec3 fPos; out vec3 fNorm; out vec3 fCol;
uniform mat4 proj, view, model;
void main(){
    vec4 wp = model * vec4(aPos,1);
    fPos = wp.xyz;
    fNorm= normalize(mat3(transpose(inverse(model)))*aNormal);
    fCol = aColor;
    gl_Position = proj*view*wp;
}
)";

static const char* actorFS = R"(
#version 330 core
in vec3 fPos; in vec3 fNorm; in vec3 fCol;
out vec4 fragColor;
uniform vec3 lightPos[3]; uniform vec3 lightColor[3]; uniform vec3 viewPos;
void main(){
    vec3 n=normalize(fNorm); vec3 res=vec3(0);
    for(int i=0;i<3;i++){
        vec3 ld=normalize(lightPos[i]-fPos);
        float d=length(lightPos[i]-fPos);
        float att=1.0/(1.0+0.07*d+0.017*d*d);
        vec3 amb=0.15*lightColor[i]*fCol;
        float df=max(dot(n,ld),0.0);
        vec3 dif=df*lightColor[i]*fCol;
        vec3 vd=normalize(viewPos-fPos);
        vec3 hd=normalize(ld+vd);
        float sp=pow(max(dot(n,hd),0.0),32.0);
        vec3 spec=sp*lightColor[i]*0.5;
        res+=amb+att*(dif+spec);
    }
    fragColor=vec4(res,1);
}
)";

// ---- Emissive shader (light orbs) ----
static const char* emitVS = R"(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 proj, view, model;
void main(){ gl_Position=proj*view*model*vec4(aPos,1); }
)";
static const char* emitFS = R"(
#version 330 core
out vec4 fragColor;
uniform vec3 emitColor;
void main(){ fragColor=vec4(emitColor,1); }
)";

// ---- Shadow pass shader ----
static const char* shadowVS = R"(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 lightSpaceMat, model;
void main(){ gl_Position=lightSpaceMat*model*vec4(aPos,1); }
)";
static const char* shadowFS = R"(
#version 330 core
void main(){}
)";

// ---- Particle shader ----
static const char* partVS = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in float aSize;
layout(location=2) in vec4 aColor;
out vec4 pColor;
uniform mat4 proj, view;
void main(){
    gl_Position = proj*view*vec4(aPos,1);
    gl_PointSize = aSize;
    pColor = aColor;
}
)";
static const char* partFS = R"(
#version 330 core
in vec4 pColor;
out vec4 fragColor;
void main(){
    vec2 c = gl_PointCoord - 0.5;
    float r = length(c);
    if(r > 0.5) discard;
    float a = pColor.a * (1.0 - r*2.0);
    fragColor = vec4(pColor.rgb, a);
}
)";

// ---- Transparent glass shader (shares roomVS) ----
static const char* glassFS = R"(
#version 330 core
in vec3 fPos; in vec3 fNorm; in vec2 fUV; in vec4 fPosLS;
out vec4 fragColor;
uniform vec3 lightPos[3]; uniform vec3 lightColor[3]; uniform vec3 viewPos;
uniform float alpha;
void main(){
    vec3 n=normalize(fNorm); vec3 res=vec3(0);
    for(int i=0;i<3;i++){
        vec3 ld=normalize(lightPos[i]-fPos);
        float d=length(lightPos[i]-fPos);
        float att=1.0/(1.0+0.07*d+0.017*d*d);
        vec3 vd=normalize(viewPos-fPos);
        vec3 hd=normalize(ld+vd);
        float sp=pow(max(dot(n,hd),0.0),128.0);
        res+=att*(0.05*lightColor[i]+sp*lightColor[i]*0.8);
    }
    vec3 glassCol=vec3(0.55,0.75,0.95);
    fragColor=vec4(glassCol+res, alpha);
}
)";

// ---------------------------------------------------------------------------
// Shader compile helpers
// ---------------------------------------------------------------------------
static GLuint compileShader(GLenum type, const char* src){
    GLuint s = glCreateShader(type);
    glShaderSource(s,1,&src,nullptr);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok){ char log[512]; glGetShaderInfoLog(s,512,nullptr,log); std::cerr<<log<<"\n"; }
    return s;
}
static GLuint linkProg(const char* vs, const char* fs){
    GLuint v=compileShader(GL_VERTEX_SHADER,vs);
    GLuint f=compileShader(GL_FRAGMENT_SHADER,fs);
    GLuint p=glCreateProgram();
    glAttachShader(p,v); glAttachShader(p,f); glLinkProgram(p);
    GLint ok; glGetProgramiv(p,GL_LINK_STATUS,&ok);
    if(!ok){ char log[512]; glGetProgramInfoLog(p,512,nullptr,log); std::cerr<<log<<"\n"; }
    glDeleteShader(v); glDeleteShader(f);
    return p;
}

// ---------------------------------------------------------------------------
// Programs
// ---------------------------------------------------------------------------
GLuint progRoom, progActor, progEmit, progShadow, progParticle, progGlass;

// ---------------------------------------------------------------------------
// Lights  (3 point lights)
// ---------------------------------------------------------------------------
glm::vec3 lightPos[3] = {
    {-9.0f,7.4f, 0.0f},
    { 0.0f,7.4f, 0.0f},
    { 9.0f,7.4f, 0.0f}
};
glm::vec3 lightColor[3] = {
    {1.0f,0.95f,0.80f},
    {1.0f,1.00f,1.00f},
    {1.0f,0.95f,0.80f}
};

// ---------------------------------------------------------------------------
// Geometry helpers
// ---------------------------------------------------------------------------
static void addRect(std::vector<Vertex>& v,
    float xa,float ya,float za, float xb,float yb,float zb,
    float xc,float yc,float zc, float xd,float yd,float zd,
    float nx,float ny,float nz,
    float u1,float v1,float u2,float v2)
{
    v.push_back({xa,ya,za,nx,ny,nz,u1,v1});
    v.push_back({xb,yb,zb,nx,ny,nz,u2,v1});
    v.push_back({xc,yc,zc,nx,ny,nz,u2,v2});
    v.push_back({xa,ya,za,nx,ny,nz,u1,v1});
    v.push_back({xc,yc,zc,nx,ny,nz,u2,v2});
    v.push_back({xd,yd,zd,nx,ny,nz,u1,v2});
}

static void addBox(std::vector<Vertex>& v,
    float cx,float cy,float cz, float sx,float sy,float sz,
    float ru,float rv)
{
    float x1=cx-sx/2,x2=cx+sx/2;
    float y1=cy-sy/2,y2=cy+sy/2;
    float z1=cz-sz/2,z2=cz+sz/2;
    addRect(v, x1,y1,z2, x1,y1,z1, x1,y2,z1, x1,y2,z2, -1,0,0, 0,0,ru,rv);
    addRect(v, x2,y1,z1, x2,y1,z2, x2,y2,z2, x2,y2,z1, +1,0,0, 0,0,ru,rv);
    addRect(v, x2,y1,z1, x1,y1,z1, x1,y2,z1, x2,y2,z1,  0,0,-1, 0,0,ru,rv);
    addRect(v, x1,y1,z2, x2,y1,z2, x2,y2,z2, x1,y2,z2,  0,0,+1, 0,0,ru,rv);
    addRect(v, x1,y1,z1, x2,y1,z1, x2,y1,z2, x1,y1,z2,  0,-1,0, 0,0,ru,rv);
    addRect(v, x1,y2,z2, x2,y2,z2, x2,y2,z1, x1,y2,z1,  0,+1,0, 0,0,ru,rv);
}

// ---------------------------------------------------------------------------
// Scene geometry
// ---------------------------------------------------------------------------
std::vector<Vertex> floorVerts, ceilingVerts, wallVerts, glassVerts;
GLuint floorVAO,ceilingVAO,wallVAO,glassVAO;
int    floorN,ceilingN,wallN,glassN;

static GLuint makeVAO(const std::vector<Vertex>& verts){
    if(verts.empty()) return 0;
    GLuint vao,vbo;
    glGenVertexArrays(1,&vao); glGenBuffers(1,&vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,verts.size()*sizeof(Vertex),verts.data(),GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)(3*4));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)(6*4));
    glBindVertexArray(0);
    return vao;
}

static void buildScene(){
    floorVerts.clear(); ceilingVerts.clear(); wallVerts.clear(); glassVerts.clear();

    const float roomW=16,roomD=16,roomH=8,corridorW=4,wallT=0.4f;
    const float cx1=-9,cx2=9,cz=0;
    float r1L=cx1-roomW/2, r1R=cx1+roomW/2;
    float r2L=cx2-roomW/2, r2R=cx2+roomW/2;
    float zL=cz-roomD/2,   zR=cz+roomD/2;
    float corZ1=-corridorW/2, corZ2=corridorW/2;
    float yB=0,yT=roomH;

    auto addFloor=[&](float x1,float z1,float x2,float z2,float y,bool fl){
        auto& t=fl?floorVerts:ceilingVerts;
        float ny=fl?1.0f:-1.0f;
        addRect(t, x1,y,z1, x2,y,z1, x2,y,z2, x1,y,z2, 0,ny,0, 0,0,8,8);
    };
    // Room 1 floor/ceiling
    addFloor(r1L,zL,r1R,zR,yB,true);  addFloor(r1L,zL,r1R,zR,yT,false);
    // Room 2
    addFloor(r2L,zL,r2R,zR,yB,true);  addFloor(r2L,zL,r2R,zR,yT,false);
    // Corridor floor
    addFloor(r1R,corZ1,r2L,corZ2,yB,true);
    // Corridor ceiling
    addFloor(r1R,corZ1,r2L,corZ2,yT,false);

    auto wb=[&](float cx,float cy,float cz2,float lx,float ly,float lz){
        addBox(wallVerts,cx,cy,cz2,lx,ly,lz,1,1);
    };
    float midY=(yB+yT)/2, hY=yT-yB;
    // Room 1
    wb((r1L+r1R)/2, midY, zL-wallT/2,   r1R-r1L, hY, wallT);
    wb((r1L+r1R)/2, midY, zR+wallT/2,   r1R-r1L, hY, wallT);
    wb(r1L-wallT/2, midY, (zL+zR)/2,    wallT,   hY, zR-zL);
    if(corZ1>zL) wb(r1R+wallT/2, midY, (zL+corZ1)/2, wallT, hY, corZ1-zL);
    if(corZ2<zR) wb(r1R+wallT/2, midY, (corZ2+zR)/2, wallT, hY, zR-corZ2);
    // Room 2
    wb((r2L+r2R)/2, midY, zL-wallT/2,   r2R-r2L, hY, wallT);
    wb((r2L+r2R)/2, midY, zR+wallT/2,   r2R-r2L, hY, wallT);
    wb(r2R+wallT/2, midY, (zL+zR)/2,    wallT,   hY, zR-zL);
    if(corZ1>zL) wb(r2L-wallT/2, midY, (zL+corZ1)/2, wallT, hY, corZ1-zL);
    if(corZ2<zR) wb(r2L-wallT/2, midY, (corZ2+zR)/2, wallT, hY, zR-corZ2);
    // Corridor walls
    wb((r1R+r2L)/2, midY, corZ1-wallT/2, r2L-r1R, hY, wallT);
    wb((r1R+r2L)/2, midY, corZ2+wallT/2, r2L-r1R, hY, wallT);

    // Furniture: 2 tables + 2 chairs (one set per room, use wallVerts/wallTex)
    auto table=[&](float cx,float cz){
        addBox(wallVerts, cx, 0.85f, cz, 2.0f, 0.08f, 1.0f, 2,1); // top
        for(float dx:{-0.85f,0.85f}) for(float dz2:{-0.42f,0.42f})
            addBox(wallVerts, cx+dx, 0.4f, cz+dz2, 0.1f,0.8f,0.1f, 1,1);
    };
    auto chair=[&](float cx,float cz){
        addBox(wallVerts, cx, 0.48f, cz,      0.55f,0.05f,0.55f, 1,1); // seat
        addBox(wallVerts, cx, 0.84f, cz+0.25f,0.55f,0.72f,0.05f, 1,1); // back
        for(float dx:{-0.22f,0.22f}) for(float dz2:{-0.22f,0.22f})
            addBox(wallVerts, cx+dx, 0.22f, cz+dz2, 0.06f,0.45f,0.06f, 1,1);
    };
    table(-9.0f, -4.5f);  chair(-9.0f, -6.5f);   // room 1
    table( 9.0f,  4.5f);  chair( 9.0f,  6.5f);   // room 2

    // Glass panels in corridor (two thin panes, both faces)
    for(float gx : {-0.3f, 0.3f}){
        // front face (normal +X)
        addRect(glassVerts, gx,0,corZ1, gx,0,corZ2, gx,yT,corZ2, gx,yT,corZ1, 1,0,0, 0,0,1,2);
        // back face (normal -X)
        addRect(glassVerts, gx,0,corZ2, gx,0,corZ1, gx,yT,corZ1, gx,yT,corZ2, -1,0,0, 0,0,1,2);
    }

    floorVAO   = makeVAO(floorVerts);   floorN   = (int)floorVerts.size();
    ceilingVAO = makeVAO(ceilingVerts); ceilingN = (int)ceilingVerts.size();
    wallVAO    = makeVAO(wallVerts);    wallN    = (int)wallVerts.size();
    glassVAO   = makeVAO(glassVerts);   glassN   = (int)glassVerts.size();
}

// ---------------------------------------------------------------------------
// PPM texture loading
// ---------------------------------------------------------------------------
static unsigned char* loadPPM(const char* fn, int& w, int& h){
    FILE* f=fopen(fn,"rb"); if(!f) return nullptr;
    char buf[256];
    if(!fgets(buf,256,f)){fclose(f);return nullptr;}
    if(buf[0]!='P'||buf[1]!='6'){fclose(f);return nullptr;}
    do{ if(!fgets(buf,256,f)){fclose(f);return nullptr;} } while(buf[0]=='#');
    sscanf(buf,"%d %d",&w,&h);
    do{ if(!fgets(buf,256,f)){fclose(f);return nullptr;} } while(buf[0]=='#');
    unsigned char* d=new unsigned char[w*h*3];
    fread(d,1,w*h*3,f); fclose(f);
    return d;
}
static GLuint loadTex(const char* fn){
    int w,h; unsigned char* img=loadPPM(fn,w,h);
    if(!img){ std::cerr<<"Cannot load "<<fn<<"\n"; return 0; }
    GLuint t; glGenTextures(1,&t); glBindTexture(GL_TEXTURE_2D,t);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,w,h,0,GL_RGB,GL_UNSIGNED_BYTE,img);
    delete[] img;
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);
    return t;
}
GLuint floorTex,wallTex,ceilingTex;

// ---------------------------------------------------------------------------
// Actor
// ---------------------------------------------------------------------------
std::vector<ColorVertex> actorVerts;
GLuint actorVAO,actorVBO; int actorN;

struct Actor { glm::vec3 pos; };
Actor actor;
std::vector<glm::vec3> path;
int   pathIdx=0; float pathT=0;
float actorSpeed=0.8f;

static void addCube(const glm::vec3& c,const glm::vec3& s,const glm::vec3& col){
    float x1=c.x-s.x/2,x2=c.x+s.x/2;
    float y1=c.y-s.y/2,y2=c.y+s.y/2;
    float z1=c.z-s.z/2,z2=c.z+s.z/2;
    auto q=[&](float xa,float ya,float za,float xb,float yb,float zb,
               float xc,float yc,float zc,float xd,float yd,float zd,
               float nx,float ny,float nz){
        actorVerts.push_back({xa,ya,za,nx,ny,nz,col.r,col.g,col.b});
        actorVerts.push_back({xb,yb,zb,nx,ny,nz,col.r,col.g,col.b});
        actorVerts.push_back({xc,yc,zc,nx,ny,nz,col.r,col.g,col.b});
        actorVerts.push_back({xa,ya,za,nx,ny,nz,col.r,col.g,col.b});
        actorVerts.push_back({xc,yc,zc,nx,ny,nz,col.r,col.g,col.b});
        actorVerts.push_back({xd,yd,zd,nx,ny,nz,col.r,col.g,col.b});
    };
    q(x1,y1,z2,x2,y1,z2,x2,y2,z2,x1,y2,z2, 0,0,+1);
    q(x2,y1,z1,x1,y1,z1,x1,y2,z1,x2,y2,z1, 0,0,-1);
    q(x1,y1,z1,x1,y1,z2,x1,y2,z2,x1,y2,z1,-1,0, 0);
    q(x2,y1,z2,x2,y1,z1,x2,y2,z1,x2,y2,z2,+1,0, 0);
    q(x1,y1,z1,x2,y1,z1,x2,y1,z2,x1,y1,z2, 0,-1,0);
    q(x1,y2,z2,x2,y2,z2,x2,y2,z1,x1,y2,z1, 0,+1,0);
}

static void buildActor(){
    actorVerts.clear();
    addCube({0,0.6f,0},{0.6f,1.0f,0.4f},{0.6f,0.4f,0.2f});
    addCube({0,1.2f,0},{0.5f,0.5f,0.4f},{0.9f,0.7f,0.4f});
    addCube({-0.45f,0.9f,0},{0.3f,0.6f,0.3f},{0.2f,0.4f,0.8f});
    addCube({ 0.45f,0.9f,0},{0.3f,0.6f,0.3f},{0.2f,0.4f,0.8f});
    addCube({-0.2f,0.1f,0},{0.3f,0.5f,0.3f},{0.1f,0.2f,0.5f});
    addCube({ 0.2f,0.1f,0},{0.3f,0.5f,0.3f},{0.1f,0.2f,0.5f});
    actorN=(int)actorVerts.size();
    glGenVertexArrays(1,&actorVAO); glGenBuffers(1,&actorVBO);
    glBindVertexArray(actorVAO);
    glBindBuffer(GL_ARRAY_BUFFER,actorVBO);
    glBufferData(GL_ARRAY_BUFFER,actorVerts.size()*sizeof(ColorVertex),actorVerts.data(),GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(ColorVertex),(void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(ColorVertex),(void*)(3*4));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,sizeof(ColorVertex),(void*)(6*4));
    glBindVertexArray(0);
}

static void initPath(){
    path={ {-12,0.1f,0},{-1.5f,0.1f,0},{1.5f,0.1f,0},{12,0.1f,0},
           {1.5f,0.1f,0},{-1.5f,0.1f,0},{-12,0.1f,0} };
    actor.pos=path[0]; pathIdx=0; pathT=0;
}

static void updateActor(float dt){
    float step=actorSpeed*dt;
    while(step>0&&pathIdx<(int)path.size()-1){
        glm::vec3 p0=path[pathIdx],p1=path[pathIdx+1];
        float seg=glm::length(p1-p0);
        float rem=seg*(1-pathT);
        if(step>=rem){ step-=rem; pathIdx++; pathT=0;
            if(pathIdx>=(int)path.size()-1){pathIdx=0;pathT=0;break;}
        } else { pathT+=step/seg; step=0; }
    }
    if(pathIdx<(int)path.size()-1)
        actor.pos=path[pathIdx]+pathT*(path[pathIdx+1]-path[pathIdx]);
    else actor.pos=path.back();
}

// ---------------------------------------------------------------------------
// Bouncing ball  (effect #2)
// ---------------------------------------------------------------------------
std::vector<ColorVertex> ballVerts;
GLuint ballVAO,ballVBO; int ballN;
glm::vec3 ballPos(-9,1.0f,3);
glm::vec3 ballVel(2.8f,4.2f,1.6f);
const float ballR=0.5f;

static void buildBall(){
    ballVerts.clear();
    const int lat=16,lon=16;
    const float pi=(float)M_PI;
    glm::vec3 col(1.0f,0.25f,0.05f);
    for(int i=0;i<lat;i++){
        float t1=pi*i/lat, t2=pi*(i+1)/lat;
        for(int j=0;j<lon;j++){
            float p1=2*pi*j/lon, p2=2*pi*(j+1)/lon;
            glm::vec3 n[4]={
                {sinf(t1)*cosf(p1),cosf(t1),sinf(t1)*sinf(p1)},
                {sinf(t1)*cosf(p2),cosf(t1),sinf(t1)*sinf(p2)},
                {sinf(t2)*cosf(p2),cosf(t2),sinf(t2)*sinf(p2)},
                {sinf(t2)*cosf(p1),cosf(t2),sinf(t2)*sinf(p1)}
            };
            // tri 0,1,2
            for(int k : {0,1,2,0,2,3})
                ballVerts.push_back({n[k].x*ballR,n[k].y*ballR,n[k].z*ballR,
                                     n[k].x,n[k].y,n[k].z, col.r,col.g,col.b});
        }
    }
    ballN=(int)ballVerts.size();
    glGenVertexArrays(1,&ballVAO); glGenBuffers(1,&ballVBO);
    glBindVertexArray(ballVAO);
    glBindBuffer(GL_ARRAY_BUFFER,ballVBO);
    glBufferData(GL_ARRAY_BUFFER,ballVerts.size()*sizeof(ColorVertex),ballVerts.data(),GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(ColorVertex),(void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(ColorVertex),(void*)(3*4));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,sizeof(ColorVertex),(void*)(6*4));
    glBindVertexArray(0);
}

static void updateBall(float dt){
    ballPos+=ballVel*dt;
    if(ballPos.y<ballR){ballPos.y=ballR;ballVel.y=fabsf(ballVel.y);}
    if(ballPos.y>8-ballR){ballPos.y=8-ballR;ballVel.y=-fabsf(ballVel.y);}
    if(ballPos.x<-17+ballR){ballPos.x=-17+ballR;ballVel.x=fabsf(ballVel.x);}
    if(ballPos.x>-1-ballR){ballPos.x=-1-ballR;ballVel.x=-fabsf(ballVel.x);}
    if(ballPos.z<-8+ballR){ballPos.z=-8+ballR;ballVel.z=fabsf(ballVel.z);}
    if(ballPos.z>8-ballR){ballPos.z=8-ballR;ballVel.z=-fabsf(ballVel.z);}
}

// ---------------------------------------------------------------------------
// Light orbs  (effect #7) — small emissive spheres
// ---------------------------------------------------------------------------
std::vector<glm::vec3> orbVerts;
GLuint orbVAO,orbVBO; int orbN;

static void buildOrb(){
    orbVerts.clear();
    const int lat=8,lon=8;
    const float pi=(float)M_PI, r=0.18f;
    for(int i=0;i<lat;i++){
        float t1=pi*i/lat,t2=pi*(i+1)/lat;
        for(int j=0;j<lon;j++){
            float p1=2*pi*j/lon,p2=2*pi*(j+1)/lon;
            glm::vec3 n[4]={
                {sinf(t1)*cosf(p1),cosf(t1),sinf(t1)*sinf(p1)},
                {sinf(t1)*cosf(p2),cosf(t1),sinf(t1)*sinf(p2)},
                {sinf(t2)*cosf(p2),cosf(t2),sinf(t2)*sinf(p2)},
                {sinf(t2)*cosf(p1),cosf(t2),sinf(t2)*sinf(p1)}
            };
            for(int k : {0,1,2,0,2,3}) orbVerts.push_back(n[k]*r);
        }
    }
    orbN=(int)orbVerts.size();
    glGenVertexArrays(1,&orbVAO); glGenBuffers(1,&orbVBO);
    glBindVertexArray(orbVAO);
    glBindBuffer(GL_ARRAY_BUFFER,orbVBO);
    glBufferData(GL_ARRAY_BUFFER,orbVerts.size()*sizeof(glm::vec3),orbVerts.data(),GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,0,nullptr);
    glBindVertexArray(0);
}

// ---------------------------------------------------------------------------
// Particles  (effect #3)
// ---------------------------------------------------------------------------
struct Particle{ glm::vec3 pos,vel; float life,maxLife,size; };
std::vector<Particle> particles;
GLuint partVAO,partVBO;
// Two fire pits — one per room, in the middle of each room.
glm::vec3 fireOrigins[2] = {
    glm::vec3(-13.0f, 0.4f,  5.0f),  // room 1: northwest area
    glm::vec3( 13.0f, 0.4f, -5.0f)   // room 2: southeast area
};

static void spawnParticle(int idx){
    Particle p;
    float rx=(rand()%100-50)/200.0f, rz=(rand()%100-50)/200.0f;
    p.pos=fireOrigins[idx]+glm::vec3(rx,0,rz);
    p.vel={(rand()%100-50)/70.0f, 2.0f+(rand()%100)/150.0f, (rand()%100-50)/70.0f};
    p.maxLife=p.life=1.0f+(rand()%100)/180.0f;
    p.size=42.0f;
    particles.push_back(p);
}
static void updateParticles(float dt){
    // 5 particles per fire per frame
    for(int i=0;i<5;i++){ spawnParticle(0); spawnParticle(1); }
    for(auto& p:particles){
        p.life-=dt; p.pos+=p.vel*dt;
        p.vel.x*=0.97f; p.vel.z*=0.97f;
        p.size=42.0f*(p.life/p.maxLife);
    }
    particles.erase(std::remove_if(particles.begin(),particles.end(),
        [](const Particle& p){return p.life<=0;}),particles.end());
    if(particles.size()>800) particles.erase(particles.begin(),particles.begin()+100);
}
static void initPartVAO(){
    glGenVertexArrays(1,&partVAO); glGenBuffers(1,&partVBO);
    glBindVertexArray(partVAO);
    glBindBuffer(GL_ARRAY_BUFFER,partVBO);
    glBufferData(GL_ARRAY_BUFFER,500*sizeof(ParticleVert),nullptr,GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(ParticleVert),(void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1,1,GL_FLOAT,GL_FALSE,sizeof(ParticleVert),(void*)(3*4));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2,4,GL_FLOAT,GL_FALSE,sizeof(ParticleVert),(void*)(4*4));
    glBindVertexArray(0);
}

// ---------------------------------------------------------------------------
// Shadow map  (effect #6)
// ---------------------------------------------------------------------------
const int SM=2048;
GLuint shadowFBO,shadowTex;
glm::mat4 lightSpaceMat;

static void initShadow(){
    glGenFramebuffers(1,&shadowFBO);
    glGenTextures(1,&shadowTex);
    glBindTexture(GL_TEXTURE_2D,shadowTex);
    glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT,SM,SM,0,GL_DEPTH_COMPONENT,GL_FLOAT,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_BORDER);
    float bc[]={1,1,1,1}; glTexParameterfv(GL_TEXTURE_2D,GL_TEXTURE_BORDER_COLOR,bc);
    glBindFramebuffer(GL_FRAMEBUFFER,shadowFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,shadowTex,0);
    glDrawBuffer(GL_NONE); glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER,0);

    // Light just below the ceiling so the ceiling is BEHIND the near clip
    // plane (otherwise it would block the entire floor from "seeing" the light).
    glm::mat4 lp=glm::ortho(-10.0f,10.0f,-10.0f,10.0f,0.3f,9.0f);
    glm::mat4 lv=glm::lookAt(glm::vec3(-9,7.3f,0),glm::vec3(-9,0,0),glm::vec3(0,0,1));
    lightSpaceMat=lp*lv;
}

// Draw opaque scene for shadow pass (uses shadow program already bound)
static void shadowPassDraw(){
    auto setModel=[&](const glm::mat4& m){
        glUniformMatrix4fv(glGetUniformLocation(progShadow,"model"),1,GL_FALSE,glm::value_ptr(m));
    };
    glm::mat4 I(1);
    setModel(I);
    glBindVertexArray(floorVAO);   glDrawArrays(GL_TRIANGLES,0,floorN);
    glBindVertexArray(wallVAO);    glDrawArrays(GL_TRIANGLES,0,wallN);
    // Ceiling intentionally skipped - it sits between the light and the floor.
    // Ball
    setModel(glm::translate(I,ballPos));
    glBindVertexArray(ballVAO); glDrawArrays(GL_TRIANGLES,0,ballN);
    // Actor
    setModel(glm::translate(I,actor.pos));
    glBindVertexArray(actorVAO); glDrawArrays(GL_TRIANGLES,0,actorN);
}

// ---------------------------------------------------------------------------
// Camera
// ---------------------------------------------------------------------------
static void updateCamera(){
    glm::vec3 front;
    front.x=cosf(glm::radians(cameraYaw))*cosf(glm::radians(cameraPitch));
    front.y=sinf(glm::radians(cameraPitch));
    front.z=sinf(glm::radians(cameraYaw))*cosf(glm::radians(cameraPitch));
    front=glm::normalize(front);
    glm::vec3 right=glm::normalize(glm::cross(front,{0,1,0}));
    glm::vec3 up=glm::cross(right,front);
    currentView=glm::lookAt(cameraPos,cameraPos+front,up);
}

static void processMovement(){
    int now=glutGet(GLUT_ELAPSED_TIME);
    float dt=std::min((now-lastTime)*0.001f,0.05f);
    lastTime=now;
    glm::vec3 front;
    front.x=cosf(glm::radians(cameraYaw))*cosf(glm::radians(cameraPitch));
    front.y=sinf(glm::radians(cameraPitch));
    front.z=sinf(glm::radians(cameraYaw))*cosf(glm::radians(cameraPitch));
    front=glm::normalize(front);
    glm::vec3 right=glm::normalize(glm::cross(front,{0,1,0}));
    float spd=moveSpeed*dt;
    glm::vec3 mv(0);
    if(keys['w']) mv+=front*spd;
    if(keys['s']) mv-=front*spd;
    if(keys['a']) mv-=right*spd;
    if(keys['d']) mv+=right*spd;
    if(keys['q']) mv.y+=spd;
    if(keys['e']) mv.y-=spd;
    if(glm::length(mv)>0){ cameraPos+=mv; glutPostRedisplay(); }
}

// ---------------------------------------------------------------------------
// Lighting uniforms helper
// ---------------------------------------------------------------------------
static void setLightUniforms(GLuint prog){
    glUniform3fv(glGetUniformLocation(prog,"lightPos"),  3,glm::value_ptr(lightPos[0]));
    glUniform3fv(glGetUniformLocation(prog,"lightColor"),3,glm::value_ptr(lightColor[0]));
    glUniform3fv(glGetUniformLocation(prog,"viewPos"),   1,glm::value_ptr(cameraPos));
}

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------
static int lastActor=0;
static float totalTime=0;
static float sceneTime=0.0f;

void display(){
    int now=glutGet(GLUT_ELAPSED_TIME);
    float dt=std::min((now-lastActor)*0.001f,0.05f);
    totalTime+=dt;
    sceneTime+=dt;
    lastActor=now;
    updateActor(dt);
    updateBall(dt);
    updateParticles(dt);
    processMovement();
    updateCamera();

    glm::mat4 proj=glm::perspective(glm::radians(60.0f),(float)windowWidth/windowHeight,0.1f,200.0f);
    glm::mat4 I(1);

    // ---- Shadow pass ----
    glViewport(0,0,SM,SM);
    glBindFramebuffer(GL_FRAMEBUFFER,shadowFBO);
    glClear(GL_DEPTH_BUFFER_BIT);
    glUseProgram(progShadow);
    glUniformMatrix4fv(glGetUniformLocation(progShadow,"lightSpaceMat"),1,GL_FALSE,glm::value_ptr(lightSpaceMat));
    shadowPassDraw();
    glBindFramebuffer(GL_FRAMEBUFFER,0);

    // ---- Main pass ----
    glViewport(0,0,windowWidth,windowHeight);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    // -- Room (floor, wall, ceiling) --
    glUseProgram(progRoom);
    glUniformMatrix4fv(glGetUniformLocation(progRoom,"proj"),1,GL_FALSE,glm::value_ptr(proj));
    glUniformMatrix4fv(glGetUniformLocation(progRoom,"view"),1,GL_FALSE,glm::value_ptr(currentView));
    glUniformMatrix4fv(glGetUniformLocation(progRoom,"model"),1,GL_FALSE,glm::value_ptr(I));
    glUniformMatrix4fv(glGetUniformLocation(progRoom,"lightSpaceMat"),1,GL_FALSE,glm::value_ptr(lightSpaceMat));
    setLightUniforms(progRoom);
    glUniform1i(glGetUniformLocation(progRoom,"useShadow"),1);
    glUniform1f(glGetUniformLocation(progRoom,"sceneTime"),sceneTime);
    // bind shadow map to unit 1
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D,shadowTex);
    glUniform1i(glGetUniformLocation(progRoom,"shadowMap"),1);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(glGetUniformLocation(progRoom,"tex"),0);

    glBindTexture(GL_TEXTURE_2D,floorTex);
    glBindVertexArray(floorVAO); glDrawArrays(GL_TRIANGLES,0,floorN);

    glBindTexture(GL_TEXTURE_2D,wallTex);
    glBindVertexArray(wallVAO); glDrawArrays(GL_TRIANGLES,0,wallN);

    glBindTexture(GL_TEXTURE_2D,ceilingTex);
    glBindVertexArray(ceilingVAO); glDrawArrays(GL_TRIANGLES,0,ceilingN);

    // -- Actor --
    glUseProgram(progActor);
    glUniformMatrix4fv(glGetUniformLocation(progActor,"proj"),1,GL_FALSE,glm::value_ptr(proj));
    glUniformMatrix4fv(glGetUniformLocation(progActor,"view"),1,GL_FALSE,glm::value_ptr(currentView));
    glm::mat4 actorM=glm::translate(I,actor.pos);
    glUniformMatrix4fv(glGetUniformLocation(progActor,"model"),1,GL_FALSE,glm::value_ptr(actorM));
    setLightUniforms(progActor);
    glBindVertexArray(actorVAO); glDrawArrays(GL_TRIANGLES,0,actorN);

    // -- Bouncing ball --
    glm::mat4 ballM=glm::translate(I,ballPos);
    glUniformMatrix4fv(glGetUniformLocation(progActor,"model"),1,GL_FALSE,glm::value_ptr(ballM));
    glBindVertexArray(ballVAO); glDrawArrays(GL_TRIANGLES,0,ballN);

    // -- Light orbs (emissive) --
    glUseProgram(progEmit);
    glUniformMatrix4fv(glGetUniformLocation(progEmit,"proj"),1,GL_FALSE,glm::value_ptr(proj));
    glUniformMatrix4fv(glGetUniformLocation(progEmit,"view"),1,GL_FALSE,glm::value_ptr(currentView));
    float pulse[3]={0.90f+0.10f*sinf(sceneTime*1.8f),
                    1.0f,
                    0.90f+0.10f*sinf(sceneTime*2.1f+1.0f)};
    for(int i=0;i<3;i++){
        glm::mat4 orbM=glm::translate(I,lightPos[i]);
        glUniformMatrix4fv(glGetUniformLocation(progEmit,"model"),1,GL_FALSE,glm::value_ptr(orbM));
        glm::vec3 ec=lightColor[i]*1.8f*pulse[i];
        glUniform3fv(glGetUniformLocation(progEmit,"emitColor"),1,glm::value_ptr(ec));
        glBindVertexArray(orbVAO); glDrawArrays(GL_TRIANGLES,0,orbN);
    }

    // -- Particles (fire) --
    if(!particles.empty()){
        std::vector<ParticleVert> pv;
        for(const auto& p:particles){
            float t=p.life/p.maxLife;
            pv.push_back({p.pos.x,p.pos.y,p.pos.z, p.size, 1.0f, t*0.55f, 0.0f, t*0.9f});
        }
        glUseProgram(progParticle);
        glUniformMatrix4fv(glGetUniformLocation(progParticle,"proj"),1,GL_FALSE,glm::value_ptr(proj));
        glUniformMatrix4fv(glGetUniformLocation(progParticle,"view"),1,GL_FALSE,glm::value_ptr(currentView));
        glBindVertexArray(partVAO);
        glBindBuffer(GL_ARRAY_BUFFER,partVBO);
        glBufferData(GL_ARRAY_BUFFER,pv.size()*sizeof(ParticleVert),pv.data(),GL_DYNAMIC_DRAW);
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE);
        glDepthMask(GL_FALSE); glEnable(GL_PROGRAM_POINT_SIZE);
        glDrawArrays(GL_POINTS,0,(int)pv.size());
        glDepthMask(GL_TRUE); glDisable(GL_BLEND); glDisable(GL_PROGRAM_POINT_SIZE);
    }

    // -- Glass panels (transparent, last) --
    glUseProgram(progGlass);
    glUniformMatrix4fv(glGetUniformLocation(progGlass,"proj"),1,GL_FALSE,glm::value_ptr(proj));
    glUniformMatrix4fv(glGetUniformLocation(progGlass,"view"),1,GL_FALSE,glm::value_ptr(currentView));
    glUniformMatrix4fv(glGetUniformLocation(progGlass,"model"),1,GL_FALSE,glm::value_ptr(I));
    glUniformMatrix4fv(glGetUniformLocation(progGlass,"lightSpaceMat"),1,GL_FALSE,glm::value_ptr(lightSpaceMat));
    setLightUniforms(progGlass);
    glUniform1f(glGetUniformLocation(progGlass,"alpha"),0.35f);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glBindVertexArray(glassVAO); glDrawArrays(GL_TRIANGLES,0,glassN);
    glDepthMask(GL_TRUE); glDisable(GL_BLEND);

    glutSwapBuffers();
    glutPostRedisplay();
}

// ---------------------------------------------------------------------------
// GLUT callbacks
// ---------------------------------------------------------------------------
void reshape(int w,int h){ windowWidth=w; windowHeight=h; glViewport(0,0,w,h); }

void mouse(int btn,int state,int x,int y){
    if(btn==GLUT_LEFT_BUTTON){
        if(state==GLUT_DOWN){mouseDown=true;mouseOldX=x;mouseOldY=y;}
        else mouseDown=false;
    } else if(btn==GLUT_RIGHT_BUTTON&&state==GLUT_DOWN){
        cameraPos={-8,2.5f,0}; cameraYaw=-90; cameraPitch=0;
    }
}
void motion(int x,int y){
    if(!mouseDown) return;
    cameraYaw  +=(x-mouseOldX)*0.2f;
    cameraPitch+=(y-mouseOldY)*0.2f;
    cameraPitch=std::max(-89.0f,std::min(89.0f,cameraPitch));
    mouseOldX=x; mouseOldY=y;
    glutPostRedisplay();
}
void keyDown(unsigned char k,int,int){
    if(k>='A'&&k<='Z') k=(unsigned char)(k+('a'-'A')); // accept Caps Lock too
    keys[k]=true; if(k==27) exit(0);
}
void keyUp(unsigned char k,int,int){
    if(k>='A'&&k<='Z') k=(unsigned char)(k+('a'-'A'));
    keys[k]=false;
}
// Arrow keys / PgUp / PgDn — backup controls in case an IME steals letter keys.
void specialDown(int k,int,int){
    if(k==GLUT_KEY_UP)        keys['w']=true;
    else if(k==GLUT_KEY_DOWN) keys['s']=true;
    else if(k==GLUT_KEY_LEFT) keys['a']=true;
    else if(k==GLUT_KEY_RIGHT)keys['d']=true;
    else if(k==GLUT_KEY_PAGE_UP)   keys['q']=true;
    else if(k==GLUT_KEY_PAGE_DOWN) keys['e']=true;
}
void specialUp(int k,int,int){
    if(k==GLUT_KEY_UP)        keys['w']=false;
    else if(k==GLUT_KEY_DOWN) keys['s']=false;
    else if(k==GLUT_KEY_LEFT) keys['a']=false;
    else if(k==GLUT_KEY_RIGHT)keys['d']=false;
    else if(k==GLUT_KEY_PAGE_UP)   keys['q']=false;
    else if(k==GLUT_KEY_PAGE_DOWN) keys['e']=false;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(int argc,char** argv){
    glutInit(&argc,argv);
    glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGB|GLUT_DEPTH);
    glutInitWindowSize(windowWidth,windowHeight);
    glutCreateWindow("CG Final Project");
    glewInit();

    glClearColor(0.05f,0.05f,0.08f,1);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    lastTime=glutGet(GLUT_ELAPSED_TIME);
    lastActor=lastTime;

    // Build programs
    progRoom     = linkProg(roomVS,  roomFS);
    progActor    = linkProg(actorVS, actorFS);
    progEmit     = linkProg(emitVS,  emitFS);
    progShadow   = linkProg(shadowVS,shadowFS);
    progParticle = linkProg(partVS,  partFS);
    progGlass    = linkProg(roomVS,  glassFS);

    if(!progRoom||!progActor||!progEmit||!progShadow||!progParticle||!progGlass){
        std::cerr<<"Shader compile failed\n"; getchar(); return 1;
    }

    // Load textures
    floorTex   = loadTex("floor.ppm");
    wallTex    = loadTex("wall.ppm");
    ceilingTex = loadTex("ceiling.ppm");
    if(!floorTex||!wallTex||!ceilingTex){
        std::cerr<<"Failed to load textures\n"; getchar(); return 1;
    }

    // Build geometry
    buildScene();
    buildActor();  initPath();
    buildBall();
    buildOrb();
    initPartVAO();
    initShadow();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyDown);
    glutKeyboardUpFunc(keyUp);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutIdleFunc([](){glutPostRedisplay();});
    glutMainLoop();
    return 0;
}
