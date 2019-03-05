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

#ifndef __OPENSPACE_MODULE_BASE___RENDERABLELINE___H__
#define __OPENSPACE_MODULE_BASE___RENDERABLELINE___H__

#include <openspace/rendering/renderable.h>

#include <openspace/properties/optionproperty.h>
#include <openspace/properties/stringproperty.h>
#include <openspace/properties/scalar/boolproperty.h>
#include <openspace/properties/scalar/floatproperty.h>
#include <ghoul/opengl/ghoul_gl.h>

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

struct LinePoint;

class RenderableLine : public Renderable {
public:
    RenderableLine(const ghoul::Dictionary& dictionary);

    void initializeGL() override;
    void deinitializeGL() override;

    bool isReady() const override;

    void render(const RenderData& data, RendererTasks& rendererTask) override;
    void update(const UpdateData& data) override;

    static documentation::Documentation Documentation();

    void createLine();

private:
    void createFilterTexture();
    void createTrianglesIndices();

private:
    properties::BoolProperty _billboard;
    properties::FloatProperty _size;
    properties::OptionProperty _blendMode;

    ghoul::opengl::ProgramObject* _shader = nullptr;

    GLuint _quad = 0u;
    GLuint _vertexPositionBuffer = 0u;
    GLuint _filterTexture = 0u;
    GLuint _indexBuffer = 0u;
    GLsizei _filterTextureSize = 0;

    static const int NUMBEROFVERTICES = 8;
    static const int NUMBEROFINDICES = 18;
    static const int MAXLINES = 1000;
    static const int MAXVERTICES = MAXLINES * NUMBEROFVERTICES;
    static const int MAXINDICES = MAXLINES * NUMBEROFINDICES;

    std::vector<unsigned int> _localIndices;
};

} // namespace openspace

#endif // __OPENSPACE_MODULE_BASE___RENDERABLELINE___H__
