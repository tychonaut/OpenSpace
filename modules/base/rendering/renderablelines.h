/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014-2019                                                               *
 *                                                                                       *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this  *
 * software and associated documentation files (the "Software"), to deal in the Software *
 * without restriction, including without limitation the rights to use, copy, modify,    *
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to    *
 * permit persons to whom the Software is furnished to do so, subject to the following   *
 * conditions:                                                                           *
 *                                                                                       *
 * The above copyright notice and this permission notice shall be included in all copies *
 * or substantial portions of the Software.                                              *
 *                                                                                       *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,   *
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A         *
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT    *
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF  *
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE  *
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                         *
 ****************************************************************************************/

#ifndef __OPENSPACE_MODULE_BASE___RenderableLines___H__
#define __OPENSPACE_MODULE_BASE___RenderableLines___H__

#include <openspace/rendering/renderable.h>

#include <openspace/properties/propertyowner.h>
#include <openspace/properties/optionproperty.h>
#include <openspace/properties/stringproperty.h>
#include <openspace/properties/scalar/boolproperty.h>
#include <openspace/properties/scalar/floatproperty.h>
#include <ghoul/opengl/ghoul_gl.h>
#include <ghoul/opengl/uniformcache.h>

#include <vector>

namespace ghoul::filesystem { class File; }

namespace ghoul::opengl {
    class ProgramObject;
    class Texture;
} // namespace ghoul::opengl

namespace openspace {

struct RenderData;
struct UpdateData;

namespace documentation { struct Documentation; }

class RenderableLines : public properties::PropertyOwner {
public:
    struct AAVertex {
        AAVertex() {};
        AAVertex(const glm::vec3& p0, const glm::vec3& p1,
            const glm::vec4& weights, float radius) :
            _p0(p0), _p1(p1), _weights(weights), _radius(radius)
        {};
        glm::vec3 _p0;
        glm::vec3 _p1;
        glm::vec4 _weights;
        float _radius;
    };
public:
    RenderableLines(const ghoul::Dictionary& dictionary);
    ~RenderableLines() = default;
    
    void initializeGL();
    void deinitializeGL();

    bool isReady() const ;

    void render(const RenderData& data);
    void update();

    static documentation::Documentation Documentation();

    void addNewLine(const glm::vec3& p0, const glm::vec3& p1, float radius);

    void reset();

    void setLineColor(const glm::vec4 color) { _lineColor = std::move(color); }
    glm::vec4 lineColor() const { return _lineColor; }

    Renderable::RenderBin renderBin() const { return _renderBinOpt; }

    void setGPUMemoryAccessType(const GLenum type) { _memoryType = std::move(type); }

private:
    void updateAspectRatio();
    void updateGPUData();
    void createFilterTexture();
    void checkGLErrors(const std::string& identifier) const;
    
protected:
    properties::OptionProperty _blendMode;
    properties::FloatProperty _opacity;

    ghoul::opengl::ProgramObject* _program = nullptr;

    GLuint _vao = 0u;
    GLuint _vbo = 0u;
    GLuint _ebo = 0u;

    GLuint _vertexPositionBuffer = 0u;
    GLuint _filterTexture = 0u;
    GLuint _indexBuffer = 0u;
    GLsizei _filterTextureSize = 16;
    
    GLfloat _aspectRatio = 1.f;
    
    bool _dataIsDirty = true;

    UniformCache(
        modelViewProjection, 
        aspectRatio, 
        lineColor, 
        filterTexture,
        opacity
    ) _uniformCache;

    std::vector<AAVertex> _verticesArray;
    std::vector<GLuint> _indicesArray;
    
    //JCC: Change this later
    glm::vec4 _lineColor = glm::vec4(1.0);

    Renderable::RenderBin _renderBinOpt;

    GLenum _memoryType = GL_STATIC_DRAW;
};

} // namespace openspace

#endif // __OPENSPACE_MODULE_BASE___RenderableLines___H__
