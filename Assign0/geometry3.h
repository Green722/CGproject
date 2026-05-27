/**** Basic setup for defining and drawing objects ****/
#ifndef __INCLUDEGEOMETRY
#define __INCLUDEGEOMETRY

#include <vector>
#include <cstdio>
#include <cmath>
#include <glm/glm.hpp>

#define PATH_TO_TEAPOT_OBJ "teapot.obj"

extern GLuint texNames[1];
extern GLuint istex;

const int numobjects = 2;
const int numperobj  = 3;
const int ncolors = 4;
GLuint VAOs[numobjects + ncolors], teapotVAO, sphereVAO;
GLuint buffers[numperobj*numobjects+ncolors+1], teapotbuffers[3], spherebuffers[3];
GLuint objects[numobjects];
GLenum PrimType[numobjects];
GLsizei NumElems[numobjects];

std::vector<glm::vec3> teapotVertices;
std::vector<glm::vec3> teapotNormals;
std::vector<unsigned int> teapotIndices;

std::vector<glm::vec3> sphereVertices;
std::vector<glm::vec3> sphereNormals;
std::vector<unsigned int> sphereIndices;

std::vector<glm::mat4> modelviewStack;

enum {Vertices, Colors, Elements};
enum {FLOOR, CUBE};

const GLfloat floorverts[4][3] = {
    {0.5,0.5,0},{-0.5,0.5,0},{-0.5,-0.5,0},{0.5,-0.5,0}
};
const GLfloat floorcol[4][3] = {
    {1,1,1},{1,1,1},{1,1,1},{1,1,1}
};
const GLubyte floorinds[1][6] = { {0,1,2,0,2,3} };
const GLfloat floortex[4][2] = {
    {1,1},{0,1},{0,0},{1,0}
};

const GLfloat wd = 0.1;
const GLfloat ht = 0.5;
const GLfloat _cubecol[4][3] = {
    {1,0,0},{0,1,0},{0,0,1},{1,1,0}
};
const GLfloat cubeverts[8][3] = {
    {-wd,-wd,0},{-wd,wd,0},{wd,wd,0},{wd,-wd,0},
    {-wd,-wd,ht},{wd,-wd,ht},{wd,wd,ht},{-wd,wd,ht}
};
GLfloat cubecol[12][3];
const GLubyte cubeinds[12][3] = {
    {0,1,2},{0,2,3},
    {4,5,6},{4,6,7},
    {0,4,7},{0,7,1},
    {0,3,5},{0,5,4},
    {3,2,6},{3,6,5},
    {1,7,6},{1,6,2}
};

const GLfloat radius = 0.1;
const int sectors = 36;
const int stacks = 18;
bool smooth;

void initobject(GLuint object, GLfloat *vert, GLint sizevert, GLfloat *col, GLint sizecol, GLubyte *inds, GLint sizeind, GLenum type);
void drawobject(GLuint object);
void initcubes(GLuint object, GLfloat *vert, GLint sizevert, GLubyte *inds, GLint sizeind, GLenum type);
void drawcolor(GLuint object, GLuint color);
void drawtexture(GLuint object, GLuint texture);
void loadteapot();
void drawteapot();
void initsphere();
void drawsphere();
void pushMatrix(glm::mat4);
void popMatrix(glm::mat4&);

void initobject(GLuint object, GLfloat *vert, GLint sizevert, GLfloat *col, GLint sizecol, GLubyte *inds, GLint sizeind, GLenum type) {
    int offset = object * numperobj;
    glBindVertexArray(VAOs[object]);
    glBindBuffer(GL_ARRAY_BUFFER, buffers[Vertices+offset]);
    glBufferData(GL_ARRAY_BUFFER, sizevert, vert, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(GLfloat),0);
    glBindBuffer(GL_ARRAY_BUFFER, buffers[Colors+offset]);
    glBufferData(GL_ARRAY_BUFFER, sizecol, col, GL_STATIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,3*sizeof(GLfloat),0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[Elements+offset]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeind, inds, GL_STATIC_DRAW);
    PrimType[object] = type;
    NumElems[object] = sizeind;
    glBindVertexArray(0);
}

void initcubes(GLuint object, GLfloat *vert, GLint sizevert, GLubyte *inds, GLint sizeind, GLenum type) {
    for (int i=0; i<ncolors; i++) {
        for (int j=0; j<8; j++)
            for (int k=0; k<3; k++)
                cubecol[j][k] = _cubecol[i][k];
        glBindVertexArray(VAOs[object+i]);
        int offset = object * numperobj;
        int base = numobjects * numperobj;
        glBindBuffer(GL_ARRAY_BUFFER, buffers[Vertices+offset]);
        glBufferData(GL_ARRAY_BUFFER, sizevert, vert, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(GLfloat),0);
        glBindBuffer(GL_ARRAY_BUFFER, buffers[base+i]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(cubecol), cubecol, GL_STATIC_DRAW);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,3*sizeof(GLfloat),0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[Elements+offset]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeind, inds, GL_STATIC_DRAW);
        PrimType[object] = type;
        NumElems[object] = sizeind;
        glBindVertexArray(0);
    }
}

void drawcolor(GLuint object, GLuint color) {
    glBindVertexArray(VAOs[object+color]);
    glDrawElements(PrimType[object], NumElems[object], GL_UNSIGNED_BYTE, 0);
    glBindVertexArray(0);
}

void drawtexture(GLuint object, GLuint texture) {
    glBindTexture(GL_TEXTURE_2D, texture);
    glBindVertexArray(VAOs[object]);
    glDrawElements(PrimType[object], NumElems[object], GL_UNSIGNED_BYTE, 0);
    glBindVertexArray(0);
}

void drawobject(GLuint object) {
    glBindVertexArray(VAOs[object]);
    glDrawElements(PrimType[object], NumElems[object], GL_UNSIGNED_BYTE, 0);
    glBindVertexArray(0);
}

void loadteapot() {
    FILE* fp = fopen(PATH_TO_TEAPOT_OBJ, "rb");
    if (!fp) { std::cerr << "Cannot open teapot.obj\n"; exit(-1); }
    float x,y,z;
    int fx,fy,fz,ignore;
    int c1,c2;
    float minY=1e9, minZ=1e9, maxY=-1e9, maxZ=-1e9;
    while (!feof(fp)) {
        c1 = fgetc(fp);
        while (!(c1=='v'||c1=='f')) { c1=fgetc(fp); if(feof(fp)) break; }
        c2 = fgetc(fp);
        if (c1=='v' && c2==' ') {
            fscanf(fp,"%f %f %f",&x,&y,&z);
            teapotVertices.push_back(glm::vec3(x,y,z));
            if(y<minY) minY=y; if(z<minZ) minZ=z;
            if(y>maxY) maxY=y; if(z>maxZ) maxZ=z;
        } else if (c1=='v' && c2=='n') {
            fscanf(fp,"%f %f %f",&x,&y,&z);
            teapotNormals.push_back(glm::normalize(glm::vec3(x,y,z)));
        } else if (c1=='f') {
            fscanf(fp,"%d//%d %d//%d %d//%d",&fx,&ignore,&fy,&ignore,&fz,&ignore);
            teapotIndices.push_back(fx-1);
            teapotIndices.push_back(fy-1);
            teapotIndices.push_back(fz-1);
        }
    }
    fclose(fp);
    float avgY = (minY+maxY)/2.0f - 0.02f;
    float avgZ = (minZ+maxZ)/2.0f;
    for (auto& v : teapotVertices) v -= glm::vec3(0, avgY, avgZ);
    
    glBindVertexArray(teapotVAO);
    glBindBuffer(GL_ARRAY_BUFFER, teapotbuffers[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3)*teapotVertices.size(), &teapotVertices[0], GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(GLfloat),0);
    glBindBuffer(GL_ARRAY_BUFFER, teapotbuffers[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3)*teapotNormals.size(), &teapotNormals[0], GL_STATIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,3*sizeof(GLfloat),0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, teapotbuffers[2]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int)*teapotIndices.size(), &teapotIndices[0], GL_STATIC_DRAW);
    glBindVertexArray(0);
}

void buildspheredata() {
    const float PI = acos(-1.0f);
    float sectorStep = 2*PI/sectors;
    float stackStep = PI/stacks;
    for (int i=0; i<=stacks; i++) {
        float stackAngle = PI/2 - i*stackStep;
        float xy = radius * cosf(stackAngle);
        float z = radius * sinf(stackAngle);
        for (int j=0; j<=sectors; j++) {
            float sectorAngle = j*sectorStep;
            float x = xy * cosf(sectorAngle);
            float y = xy * sinf(sectorAngle);
            sphereVertices.push_back(glm::vec3(x,y,z));
            glm::vec3 norm = glm::normalize(glm::vec3(x,y,z));
            sphereNormals.push_back(norm);
        }
    }
    for (int i=0; i<stacks; i++) {
        int k1 = i*(sectors+1);
        int k2 = k1 + sectors + 1;
        for (int j=0; j<sectors; j++, k1++, k2++) {
            if (i!=0) {
                sphereIndices.push_back(k1);
                sphereIndices.push_back(k2);
                sphereIndices.push_back(k1+1);
            }
            if (i!=stacks-1) {
                sphereIndices.push_back(k1+1);
                sphereIndices.push_back(k2);
                sphereIndices.push_back(k2+1);
            }
        }
    }
}

void initsphere() {
    buildspheredata();
    glBindVertexArray(sphereVAO);
    glBindBuffer(GL_ARRAY_BUFFER, spherebuffers[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3)*sphereVertices.size(), &sphereVertices[0], GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(GLfloat),0);
    glBindBuffer(GL_ARRAY_BUFFER, spherebuffers[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3)*sphereNormals.size(), &sphereNormals[0], GL_STATIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,3*sizeof(GLfloat),0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, spherebuffers[2]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int)*sphereIndices.size(), &sphereIndices[0], GL_STATIC_DRAW);
    glBindVertexArray(0);
}

void drawteapot() {
    glBindVertexArray(teapotVAO);
    glDrawElements(GL_TRIANGLES, teapotIndices.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void drawsphere() {
    glBindVertexArray(sphereVAO);
    glDrawElements(GL_TRIANGLES, sphereIndices.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void pushMatrix(glm::mat4 mat) {
    modelviewStack.push_back(mat);
}

void popMatrix(glm::mat4& mat) {
    if (modelviewStack.size()) {
        mat = modelviewStack.back();
        modelviewStack.pop_back();
    } else mat = glm::mat4(1.0f);
}

#endif