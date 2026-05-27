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

// 窗口大小
int windowWidth = 1024, windowHeight = 768;

// 着色器程序
GLuint shaderProgram;
GLuint projectionLoc, modelviewLoc, texLoc;

// 纹理 ID
GLuint floorTex, wallTex, ceilingTex;

// 相机控制
glm::vec3 cameraPos = glm::vec3(-8.0f, 2.5f, 0.0f);
float cameraYaw = -90.0f;
float cameraPitch = 0.0f;
bool mouseDown = false;
int mouseOldX, mouseOldY;
float moveSpeed = 6.0f;
int lastTime = 0;
bool keys[256] = { false };
glm::mat4 currentView;  // 存储当前相机视图矩阵

// 几何数据
struct Vertex {
    float x, y, z;
    float u, v;
};
std::vector<Vertex> floorVerts;
std::vector<Vertex> ceilingVerts;
std::vector<Vertex> wallVerts;

// ========== 演员定义 ==========
struct Actor {
    glm::vec3 position;
};
Actor actor;
std::vector<glm::vec3> path;
int currentPathIndex = 0;
float pathT = 0.0f;
float actorSpeed = 0.8f;   // 移动速度 (米/秒)

// 演员专用的着色器程序
GLuint actorShader;
GLuint actorProjectionLoc, actorModelviewLoc;

// 演员顶点数据（立方体拼凑）
struct ColorVertex {
    float x, y, z;
    float r, g, b;
};
std::vector<ColorVertex> actorVertices;
GLuint actorVAO, actorVBO;
int actorVertexCount;

// 添加一个彩色立方体到 actorVertices
void addColoredCube(const glm::vec3& center, const glm::vec3& size, const glm::vec3& color) {
    float x1 = center.x - size.x / 2, x2 = center.x + size.x / 2;
    float y1 = center.y - size.y / 2, y2 = center.y + size.y / 2;
    float z1 = center.z - size.z / 2, z2 = center.z + size.z / 2;
    auto addQuad = [&](float xa, float ya, float za, float xb, float yb, float zb, float xc, float yc, float zc, float xd, float yd, float zd) {
        actorVertices.push_back({ xa, ya, za, color.r, color.g, color.b });
        actorVertices.push_back({ xb, yb, zb, color.r, color.g, color.b });
        actorVertices.push_back({ xc, yc, zc, color.r, color.g, color.b });
        actorVertices.push_back({ xa, ya, za, color.r, color.g, color.b });
        actorVertices.push_back({ xc, yc, zc, color.r, color.g, color.b });
        actorVertices.push_back({ xd, yd, zd, color.r, color.g, color.b });
        };
    // 前面 (z = z2)
    addQuad(x1, y1, z2, x2, y1, z2, x2, y2, z2, x1, y2, z2);
    // 后面 (z = z1)
    addQuad(x2, y1, z1, x1, y1, z1, x1, y2, z1, x2, y2, z1);
    // 左面 (x = x1)
    addQuad(x1, y1, z1, x1, y1, z2, x1, y2, z2, x1, y2, z1);
    // 右面 (x = x2)
    addQuad(x2, y1, z2, x2, y1, z1, x2, y2, z1, x2, y2, z2);
    // 下面 (y = y1)
    addQuad(x1, y1, z1, x2, y1, z1, x2, y1, z2, x1, y1, z2);
    // 上面 (y = y2)
    addQuad(x1, y2, z2, x2, y2, z2, x2, y2, z1, x1, y2, z1);
}

// 创建演员模型（立方体拼成的简单人形）
void createActorModel() {
    actorVertices.clear();
    // 身体 (棕色)
    glm::vec3 bodyCenter(0, 0.6f, 0);
    glm::vec3 bodySize(0.6f, 1.0f, 0.4f);
    addColoredCube(bodyCenter, bodySize, glm::vec3(0.6f, 0.4f, 0.2f));
    // 头 (浅棕色)
    glm::vec3 headCenter(0, 1.2f, 0);
    glm::vec3 headSize(0.5f, 0.5f, 0.4f);
    addColoredCube(headCenter, headSize, glm::vec3(0.9f, 0.7f, 0.4f));
    // 左臂 (蓝色)
    glm::vec3 leftArmCenter(-0.45f, 0.9f, 0);
    glm::vec3 armSize(0.3f, 0.6f, 0.3f);
    addColoredCube(leftArmCenter, armSize, glm::vec3(0.2f, 0.4f, 0.8f));
    // 右臂
    glm::vec3 rightArmCenter(0.45f, 0.9f, 0);
    addColoredCube(rightArmCenter, armSize, glm::vec3(0.2f, 0.4f, 0.8f));
    // 左腿 (深蓝)
    glm::vec3 leftLegCenter(-0.2f, 0.1f, 0);
    glm::vec3 legSize(0.3f, 0.5f, 0.3f);
    addColoredCube(leftLegCenter, legSize, glm::vec3(0.1f, 0.2f, 0.5f));
    // 右腿
    glm::vec3 rightLegCenter(0.2f, 0.1f, 0);
    addColoredCube(rightLegCenter, legSize, glm::vec3(0.1f, 0.2f, 0.5f));
    actorVertexCount = actorVertices.size();

    glGenVertexArrays(1, &actorVAO);
    glGenBuffers(1, &actorVBO);
    glBindVertexArray(actorVAO);
    glBindBuffer(GL_ARRAY_BUFFER, actorVBO);
    glBufferData(GL_ARRAY_BUFFER, actorVertices.size() * sizeof(ColorVertex), actorVertices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ColorVertex), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ColorVertex), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);
}

// 演员专用着色器（位置+颜色）
GLuint createActorShader() {
    const char* vertSrc =
        "#version 330 core\n"
        "layout(location=0) in vec3 aPos;\n"
        "layout(location=1) in vec3 aColor;\n"
        "out vec3 vColor;\n"
        "uniform mat4 projection;\n"
        "uniform mat4 modelview;\n"
        "void main() {\n"
        "    gl_Position = projection * modelview * vec4(aPos, 1.0);\n"
        "    vColor = aColor;\n"
        "}\n";
    const char* fragSrc =
        "#version 330 core\n"
        "in vec3 vColor;\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "    fragColor = vec4(vColor, 1.0);\n"
        "}\n";
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertSrc, NULL);
    glCompileShader(vs);
    GLint compiled;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &compiled);
    if (!compiled) { char log[512]; glGetShaderInfoLog(vs, 512, NULL, log); std::cerr << "Actor VS error: " << log << std::endl; return 0; }
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragSrc, NULL);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &compiled);
    if (!compiled) { char log[512]; glGetShaderInfoLog(fs, 512, NULL, log); std::cerr << "Actor FS error: " << log << std::endl; return 0; }
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs); glDeleteShader(fs);
    return prog;
}

// 定义路径点（世界坐标，y 固定在地面以上0.1米）
void initPath() {
    path.clear();
    // 房间1起点
    path.push_back(glm::vec3(-12.0f, 0.1f, 0.0f));
    // 穿过走廊
    path.push_back(glm::vec3(-1.5f, 0.1f, 0.0f));
    path.push_back(glm::vec3(1.5f, 0.1f, 0.0f));
    // 进入房间2
    path.push_back(glm::vec3(12.0f, 0.1f, 0.0f));
    // 返回
    path.push_back(glm::vec3(1.5f, 0.1f, 0.0f));
    path.push_back(glm::vec3(-1.5f, 0.1f, 0.0f));
    path.push_back(glm::vec3(-12.0f, 0.1f, 0.0f));

    actor.position = path[0];
    currentPathIndex = 0;
    pathT = 0.0f;
}

// 更新演员位置（基于时间差）
void updateActor(float deltaTime) {
    if (path.empty()) return;
    float step = actorSpeed * deltaTime;
    while (step > 0.0f && currentPathIndex < (int)path.size() - 1) {
        glm::vec3 p0 = path[currentPathIndex];
        glm::vec3 p1 = path[currentPathIndex + 1];
        float segLen = glm::length(p1 - p0);
        float remain = segLen - pathT * segLen;
        if (step >= remain) {
            step -= remain;
            pathT = 1.0f;
            currentPathIndex++;
            if (currentPathIndex >= (int)path.size() - 1) {
                currentPathIndex = 0;
                pathT = 0.0f;
                break;
            }
        }
        else {
            pathT += step / segLen;
            step = 0.0f;
        }
    }
    if (currentPathIndex < (int)path.size() - 1) {
        glm::vec3 p0 = path[currentPathIndex];
        glm::vec3 p1 = path[currentPathIndex + 1];
        actor.position = p0 + pathT * (p1 - p0);
    }
    else {
        actor.position = path.back();
    }
}

// ========== PPM 纹理加载 ==========
unsigned char* loadPPM(const char* filename, int& width, int& height) {
    FILE* f = fopen(filename, "rb");
    if (!f) return nullptr;
    char buf[1024];
    if (!fgets(buf, sizeof(buf), f)) { fclose(f); return nullptr; }
    if (buf[0] != 'P' || buf[1] != '6') { fclose(f); return nullptr; }
    do { if (!fgets(buf, sizeof(buf), f)) { fclose(f); return nullptr; } } while (buf[0] == '#');
    sscanf(buf, "%d %d", &width, &height);
    do { if (!fgets(buf, sizeof(buf), f)) { fclose(f); return nullptr; } } while (buf[0] == '#');
    int maxval;
    sscanf(buf, "%d", &maxval);
    unsigned char* data = new unsigned char[width * height * 3];
    fread(data, 1, width * height * 3, f);
    fclose(f);
    return data;
}

GLuint loadTexturePPM(const char* filename) {
    int width, height;
    unsigned char* image = loadPPM(filename, width, height);
    if (!image) {
        std::cerr << "Failed to load PPM texture: " << filename << std::endl;
        return 0;
    }
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
    delete[] image;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);
    return tex;
}

// 矩形面
void addRect(std::vector<Vertex>& verts,
    float x1, float y1, float z1,
    float x2, float y2, float z2,
    bool isXAligned, bool isZAligned,
    float u1, float v1, float u2, float v2) {
    float xa, ya, za, xb, yb, zb, xc, yc, zc, xd, yd, zd;
    if (isZAligned) {
        xa = x1; ya = y1; za = z1;
        xb = x2; yb = y1; zb = z1;
        xc = x2; yc = y2; zc = z2;
        xd = x1; yd = y2; zd = z2;
    }
    else if (isXAligned) {
        xa = x1; ya = y1; za = z1;
        xb = x1; yb = y1; zb = z2;
        xc = x1; yc = y2; zc = z2;
        xd = x1; yd = y2; zd = z1;
    }
    else {
        xa = x1; ya = y1; za = z1;
        xb = x2; yb = y1; zb = z1;
        xc = x2; yc = y2; zc = z2;
        xd = x1; yd = y2; zd = z2;
    }
    verts.push_back({ xa, ya, za, u1, v1 });
    verts.push_back({ xb, yb, zb, u2, v1 });
    verts.push_back({ xc, yc, zc, u2, v2 });
    verts.push_back({ xa, ya, za, u1, v1 });
    verts.push_back({ xc, yc, zc, u2, v2 });
    verts.push_back({ xd, yd, zd, u1, v2 });
}

void addBox(std::vector<Vertex>& verts,
    float cx, float cy, float cz,
    float sx, float sy, float sz,
    float repeatU, float repeatV) {
    float x1 = cx - sx / 2, x2 = cx + sx / 2;
    float y1 = cy - sy / 2, y2 = cy + sy / 2;
    float z1 = cz - sz / 2, z2 = cz + sz / 2;
    addRect(verts, x1, y1, z1, x1, y2, z2, true, false, 0, 0, repeatU, repeatV);
    addRect(verts, x2, y1, z1, x2, y2, z2, true, false, 0, 0, repeatU, repeatV);
    addRect(verts, x1, y1, z1, x2, y2, z1, false, true, 0, 0, repeatU, repeatV);
    addRect(verts, x1, y1, z2, x2, y2, z2, false, true, 0, 0, repeatU, repeatV);
    addRect(verts, x1, y1, z1, x2, y1, z2, false, false, 0, 0, repeatU, repeatV);
    addRect(verts, x1, y2, z1, x2, y2, z2, false, false, 0, 0, repeatU, repeatV);
}

// 构建场景（墙体）
void buildScene() {
    floorVerts.clear();
    ceilingVerts.clear();
    wallVerts.clear();

    float roomW = 16.0f, roomD = 16.0f, roomH = 8.0f;
    float corridorW = 4.0f;
    float wallThick = 0.4f;
    float roomCX1 = -9.0f, roomCX2 = 9.0f;
    float roomCZ = 0.0f;
    float r1_xL = roomCX1 - roomW / 2, r1_xR = roomCX1 + roomW / 2;
    float r2_xL = roomCX2 - roomW / 2, r2_xR = roomCX2 + roomW / 2;
    float zL = roomCZ - roomD / 2, zR = roomCZ + roomD / 2;
    float yBottom = 0.0f, yTop = roomH;
    float corZ1 = -corridorW / 2, corZ2 = corridorW / 2;

    auto addFloorCeil = [&](float x1, float z1, float x2, float z2, float y, bool isFloor) {
        std::vector<Vertex>& target = isFloor ? floorVerts : ceilingVerts;
        addRect(target, x1, y, z1, x2, y, z2, false, false, 0, 0, 8, 8);
        };
    addFloorCeil(r1_xL, zL, r1_xR, zR, yBottom, true);
    addFloorCeil(r1_xL, zL, r1_xR, zR, yTop, false);
    addFloorCeil(r2_xL, zL, r2_xR, zR, yBottom, true);
    addFloorCeil(r2_xL, zL, r2_xR, zR, yTop, false);
    addFloorCeil(r1_xR, corZ1, r2_xL, corZ2, yBottom, true);

    auto addWallBlock = [&](float cx, float cy, float cz, float lenX, float lenY, float lenZ, float repU, float repV) {
        addBox(wallVerts, cx, cy, cz, lenX, lenY, lenZ, repU, repV);
        };

    // 房间1 后墙
    addWallBlock((r1_xL + r1_xR) / 2, (yBottom + yTop) / 2, zL - wallThick / 2, r1_xR - r1_xL, yTop - yBottom, wallThick, 1, 1);
    // 前墙
    addWallBlock((r1_xL + r1_xR) / 2, (yBottom + yTop) / 2, zR + wallThick / 2, r1_xR - r1_xL, yTop - yBottom, wallThick, 1, 1);
    // 左墙
    addWallBlock(r1_xL - wallThick / 2, (yBottom + yTop) / 2, (zL + zR) / 2, wallThick, yTop - yBottom, zR - zL, 1, 1);
    // 右墙分段
    if (corZ1 > zL) addWallBlock(r1_xR + wallThick / 2, (yBottom + yTop) / 2, (zL + corZ1) / 2, wallThick, yTop - yBottom, corZ1 - zL, 1, 1);
    if (corZ2 < zR) addWallBlock(r1_xR + wallThick / 2, (yBottom + yTop) / 2, (corZ2 + zR) / 2, wallThick, yTop - yBottom, zR - corZ2, 1, 1);
    // 房间2
    addWallBlock((r2_xL + r2_xR) / 2, (yBottom + yTop) / 2, zL - wallThick / 2, r2_xR - r2_xL, yTop - yBottom, wallThick, 1, 1);
    addWallBlock((r2_xL + r2_xR) / 2, (yBottom + yTop) / 2, zR + wallThick / 2, r2_xR - r2_xL, yTop - yBottom, wallThick, 1, 1);
    addWallBlock(r2_xR + wallThick / 2, (yBottom + yTop) / 2, (zL + zR) / 2, wallThick, yTop - yBottom, zR - zL, 1, 1);
    if (corZ1 > zL) addWallBlock(r2_xL - wallThick / 2, (yBottom + yTop) / 2, (zL + corZ1) / 2, wallThick, yTop - yBottom, corZ1 - zL, 1, 1);
    if (corZ2 < zR) addWallBlock(r2_xL - wallThick / 2, (yBottom + yTop) / 2, (corZ2 + zR) / 2, wallThick, yTop - yBottom, zR - corZ2, 1, 1);
    // 走廊墙壁
    addWallBlock((r1_xR + r2_xL) / 2, (yBottom + yTop) / 2, corZ1 - wallThick / 2, r2_xL - r1_xR, yTop - yBottom, wallThick, 1, 1);
    addWallBlock((r1_xR + r2_xL) / 2, (yBottom + yTop) / 2, corZ2 + wallThick / 2, r2_xL - r1_xR, yTop - yBottom, wallThick, 1, 1);
}

// 创建 VAO
GLuint createVAO(const std::vector<Vertex>& verts) {
    if (verts.empty()) return 0;
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);
    return vao;
}

// 房间着色器
GLuint createShaderProgram() {
    const char* vertSrc =
        "#version 330 core\n"
        "layout(location=0) in vec3 aPos;\n"
        "layout(location=1) in vec2 aTexCoord;\n"
        "out vec2 vTexCoord;\n"
        "uniform mat4 projection;\n"
        "uniform mat4 modelview;\n"
        "void main() {\n"
        "    gl_Position = projection * modelview * vec4(aPos, 1.0);\n"
        "    vTexCoord = aTexCoord;\n"
        "}\n";
    const char* fragSrc =
        "#version 330 core\n"
        "in vec2 vTexCoord;\n"
        "out vec4 fragColor;\n"
        "uniform sampler2D tex;\n"
        "void main() {\n"
        "    fragColor = texture(tex, vTexCoord);\n"
        "}\n";
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertSrc, NULL);
    glCompileShader(vs);
    GLint compiled;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &compiled);
    if (!compiled) { char log[512]; glGetShaderInfoLog(vs, 512, NULL, log); std::cerr << "VS error: " << log << std::endl; return 0; }
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragSrc, NULL);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &compiled);
    if (!compiled) { char log[512]; glGetShaderInfoLog(fs, 512, NULL, log); std::cerr << "FS error: " << log << std::endl; return 0; }
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint linked;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (!linked) { char log[512]; glGetProgramInfoLog(prog, 512, NULL, log); std::cerr << "Link error: " << log << std::endl; return 0; }
    glDeleteShader(vs); glDeleteShader(fs);
    return prog;
}

// 更新相机并保存当前视图矩阵
void updateCamera() {
    glm::vec3 front;
    front.x = cos(glm::radians(cameraYaw)) * cos(glm::radians(cameraPitch));
    front.y = sin(glm::radians(cameraPitch));
    front.z = sin(glm::radians(cameraYaw)) * cos(glm::radians(cameraPitch));
    front = glm::normalize(front);
    glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0, 1, 0)));
    glm::vec3 up = glm::cross(right, front);
    currentView = glm::lookAt(cameraPos, cameraPos + front, up);
    glUniformMatrix4fv(modelviewLoc, 1, GL_FALSE, glm::value_ptr(currentView));
}

void processMovement() {
    int now = glutGet(GLUT_ELAPSED_TIME);
    float delta = (now - lastTime) * 0.001f;
    lastTime = now;
    if (delta > 0.05f) delta = 0.05f;
    float speed = moveSpeed * delta;
    glm::vec3 front;
    front.x = cos(glm::radians(cameraYaw)) * cos(glm::radians(cameraPitch));
    front.y = sin(glm::radians(cameraPitch));
    front.z = sin(glm::radians(cameraYaw)) * cos(glm::radians(cameraPitch));
    front = glm::normalize(front);
    glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0, 1, 0)));
    float dx = 0, dz = 0, dy = 0;
    if (keys['w']) { dx += front.x * speed; dz += front.z * speed; }
    if (keys['s']) { dx -= front.x * speed; dz -= front.z * speed; }
    if (keys['a']) { dx -= right.x * speed; dz -= right.z * speed; }
    if (keys['d']) { dx += right.x * speed; dz += right.z * speed; }
    if (keys['q']) dy += speed;
    if (keys['e']) dy -= speed;
    if (dx != 0 || dz != 0 || dy != 0) {
        cameraPos += glm::vec3(dx, dy, dz);
        glutPostRedisplay();
    }
}

GLuint floorVAO, ceilingVAO, wallVAO;
int floorVertCount, ceilingVertCount, wallVertCount;
int lastActorUpdate = 0;

void display() {
    // 更新演员位置
    int nowTime = glutGet(GLUT_ELAPSED_TIME);
    float delta = (nowTime - lastActorUpdate) * 0.001f;
    lastActorUpdate = nowTime;
    if (delta > 0.05f) delta = 0.05f;
    updateActor(delta);

    processMovement();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 绘制房间
    glUseProgram(shaderProgram);
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), (float)windowWidth / windowHeight, 0.1f, 100.0f);
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(proj));
    updateCamera();  // 设置房间的 modelview 并存储 currentView
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(texLoc, 0);

    glBindTexture(GL_TEXTURE_2D, floorTex);
    glBindVertexArray(floorVAO);
    glDrawArrays(GL_TRIANGLES, 0, floorVertCount);

    glBindTexture(GL_TEXTURE_2D, wallTex);
    glBindVertexArray(wallVAO);
    glDrawArrays(GL_TRIANGLES, 0, wallVertCount);

    glBindTexture(GL_TEXTURE_2D, ceilingTex);
    glBindVertexArray(ceilingVAO);
    glDrawArrays(GL_TRIANGLES, 0, ceilingVertCount);

    // 绘制演员
    glUseProgram(actorShader);
    glUniformMatrix4fv(actorProjectionLoc, 1, GL_FALSE, glm::value_ptr(proj));
    glm::mat4 actorModel = glm::translate(glm::mat4(1.0f), actor.position);
    glm::mat4 actorMV = currentView * actorModel;
    glUniformMatrix4fv(actorModelviewLoc, 1, GL_FALSE, glm::value_ptr(actorMV));
    glBindVertexArray(actorVAO);
    glDrawArrays(GL_TRIANGLES, 0, actorVertexCount);

    glutSwapBuffers();
}

// 鼠标回调
void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON) {
        if (state == GLUT_DOWN) { mouseDown = true; mouseOldX = x; mouseOldY = y; }
        else mouseDown = false;
    }
    else if (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN) {
        cameraPos = glm::vec3(-8.0f, 2.5f, 0.0f);
        cameraYaw = -90.0f; cameraPitch = 0.0f;
        glutPostRedisplay();
    }
}

void motion(int x, int y) {
    if (!mouseDown) return;
    int dx = x - mouseOldX, dy = y - mouseOldY;
    cameraYaw += dx * 0.2f;
    cameraPitch += dy * 0.2f;
    if (cameraPitch > 89.0f) cameraPitch = 89.0f;
    if (cameraPitch < -89.0f) cameraPitch = -89.0f;
    mouseOldX = x; mouseOldY = y;
    glutPostRedisplay();
}

void keyboardDown(unsigned char key, int x, int y) {
    keys[key] = true;
    if (key == 27) exit(0);
}
void keyboardUp(unsigned char key, int x, int y) {
    keys[key] = false;
}

void reshape(int w, int h) {
    windowWidth = w; windowHeight = h;
    glViewport(0, 0, w, h);
}

void initGL() {
    glClearColor(0.2f, 0.2f, 0.2f, 1);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    lastTime = glutGet(GLUT_ELAPSED_TIME);
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(windowWidth, windowHeight);
    glutCreateWindow("Two Rooms with Walking Actor");
    glewInit();

    // 房间着色器
    shaderProgram = createShaderProgram();
    if (shaderProgram == 0) return 1;
    glUseProgram(shaderProgram);
    projectionLoc = glGetUniformLocation(shaderProgram, "projection");
    modelviewLoc = glGetUniformLocation(shaderProgram, "modelview");
    texLoc = glGetUniformLocation(shaderProgram, "tex");

    floorTex = loadTexturePPM("floor.ppm");
    wallTex = loadTexturePPM("wall.ppm");
    ceilingTex = loadTexturePPM("ceiling.ppm");
    if (floorTex == 0 || wallTex == 0 || ceilingTex == 0) {
        std::cerr << "Failed to load floor.ppm, wall.ppm, or ceiling.ppm" << std::endl;
        getchar();
        return 1;
    }

    // 演员着色器
    actorShader = createActorShader();
    if (actorShader == 0) return 1;
    glUseProgram(actorShader);
    actorProjectionLoc = glGetUniformLocation(actorShader, "projection");
    actorModelviewLoc = glGetUniformLocation(actorShader, "modelview");

    // 构建场景和演员模型
    buildScene();
    createActorModel();
    initPath();

    floorVAO = createVAO(floorVerts);
    ceilingVAO = createVAO(ceilingVerts);
    wallVAO = createVAO(wallVerts);
    floorVertCount = (int)floorVerts.size();
    ceilingVertCount = (int)ceilingVerts.size();
    wallVertCount = (int)wallVerts.size();

    initGL();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboardDown);
    glutKeyboardUpFunc(keyboardUp);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutIdleFunc([]() { glutPostRedisplay(); });
    glutMainLoop();
    return 0;
}