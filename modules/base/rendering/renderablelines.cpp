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

#include <modules/base/rendering/renderablelines.h>

#include <modules/base/basemodule.h>
#include <openspace/documentation/documentation.h>
#include <openspace/documentation/verifier.h>
#include <openspace/engine/globals.h>
#include <openspace/engine/windowdelegate.h>
#include <openspace/rendering/renderengine.h>
#include <openspace/scene/scenegraphnode.h>
#include <openspace/util/updatestructures.h>
#include <ghoul/filesystem/filesystem.h>
#include <ghoul/io/texture/texturereader.h>
#include <ghoul/misc/defer.h>
#include <ghoul/opengl/programobject.h>
#include <ghoul/opengl/texture.h>
#include <ghoul/opengl/textureunit.h>
#include <ghoul/glm.h>
#include <glm/gtx/string_cast.hpp>

#include <algorithm>

namespace {
    constexpr const char* _loggerCat = "RenderableLines";

    constexpr const char* ProgramName = "AALine";

    enum BlendMode {
        BlendModeNormal = 0,
        BlendModeAdditive
    };

    constexpr const std::array<const char*, 4> UniformNames = {
        "modelViewProjection", "aspectRatio", "lineColor", "filterTexture"
    };

    constexpr openspace::properties::Property::PropertyInfo FilteringTextureSizeInfo = {
        "FilteringTextureSize",
        "Filtering Texture Size (in pixels)",
        "This value specifies the size (in pixels) of the filtering texture for " 
        "the Antialized lines."
    };

    constexpr openspace::properties::Property::PropertyInfo BlendModeInfo = {
        "BlendMode",
        "Blending Mode",
        "This determines the blending mode that is applied to this plane."
    };
} // namespace

namespace openspace {

documentation::Documentation RenderableLines::Documentation() {
    using namespace documentation;
    return {
        "Renderable Lines",
        "base_renderable_lines",
        {
            {
                FilteringTextureSizeInfo.identifier,
                new IntVerifier,
                Optional::Yes,
                FilteringTextureSizeInfo.description
            },
            {
                BlendModeInfo.identifier,
                new StringInListVerifier({ "Normal", "Additive" }),
                Optional::Yes,
                BlendModeInfo.description, // + " The default value is 'Normal'.",
            }
        }
    };
}

RenderableLines::RenderableLines(const ghoul::Dictionary& dictionary)
    : Renderable(dictionary)
    , _blendMode(BlendModeInfo, properties::OptionProperty::DisplayType::Dropdown)
{
    documentation::testSpecificationAndThrow(
        Documentation(),
        dictionary,
        "RenderableLines"
    );

    addProperty(_opacity);
    registerUpdateRenderBinFromOpacity();

    _blendMode.addOptions({
        { BlendModeNormal, "Normal" },
        { BlendModeAdditive, "Additive"}
        });
        
    _blendMode.onChange([&]() {
        switch (_blendMode) {
        case BlendModeNormal:
            setRenderBin(Renderable::RenderBin::Opaque);
            break;
        case BlendModeAdditive:
            setRenderBin(Renderable::RenderBin::Transparent);
            break;
        default:
            throw ghoul::MissingCaseException();
        }
    });

    if (dictionary.hasKey(FilteringTextureSizeInfo.identifier)) {
        _filterTextureSize = dictionary.value<int>(FilteringTextureSizeInfo.identifier);
    }

    if (dictionary.hasKey(BlendModeInfo.identifier)) {
        const std::string v = dictionary.value<std::string>(BlendModeInfo.identifier);
        if (v == "Normal") {
            _blendMode = BlendModeNormal;
        }
        else if (v == "Additive") {
            _blendMode = BlendModeAdditive;
        }
    }
}

bool RenderableLines::isReady() const {
    return _program != nullptr;
}

void RenderableLines::initializeGL() {  
    _program = BaseModule::ProgramObjectManager.request(
        ProgramName,
        []() -> std::unique_ptr<ghoul::opengl::ProgramObject> {
        return global::renderEngine.buildRenderProgram(
            ProgramName,
            absPath("${MODULE_BASE}/shaders/aaline_vs.glsl"),
            absPath("${MODULE_BASE}/shaders/aaline_fs.glsl")
        );
    }
    );

    ghoul::opengl::updateUniformLocations(*_program, _uniformCache, UniformNames);

    createFilterTexture();

    updateAspectRatio();

    updateGPUData();

    _dataIsDirty = false;
}

void RenderableLines::deinitializeGL() {
    if (_vao != 0u) {
        glDeleteVertexArrays(1, &_vao);
        glDeleteBuffers(1, &_vbo);
        glDeleteBuffers(1, &_ebo);
    }
        
    _indicesArray.clear();
    _verticesArray.clear();

    glDeleteTextures(1, &_filterTexture);

    BaseModule::ProgramObjectManager.release(
        ProgramName,
        [](ghoul::opengl::ProgramObject* p) {
        global::renderEngine.removeRenderProgram(p);
    }
    );

    _program = nullptr;
}

void RenderableLines::render(const RenderData& data, RendererTasks&) {
    checkGLErrors("before rendering");
    _program->activate();

    //_program->setUniform("opacity", _opacity);

    const glm::dmat4 modelTransform =
        glm::translate(glm::dmat4(1.0), data.modelTransform.translation) *
        glm::dmat4(data.modelTransform.rotation) *
        glm::scale(glm::dmat4(1.0), glm::dvec3(data.modelTransform.scale)) *
        glm::dmat4(1.0);
    const glm::dmat4 modelViewTransform =
        data.camera.combinedViewMatrix() * modelTransform;    

    bool usingFramebufferRenderer = global::renderEngine.rendererImplementation() ==
        RenderEngine::RendererImplementation::Framebuffer;

    bool usingABufferRenderer = global::renderEngine.rendererImplementation() ==
        RenderEngine::RendererImplementation::ABuffer;

    if (usingABufferRenderer) {
        _program->setUniform("additiveBlending", _blendMode == BlendModeAdditive);
    }

    bool additiveBlending = (_blendMode == BlendModeAdditive) && usingFramebufferRenderer;
    if (additiveBlending) {
        glDepthMask(false);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    }

    _program->setUniform(_uniformCache.lineColor, _lineColor);
    _program->setUniform(
        _uniformCache.modelViewProjection, 
        glm::mat4(glm::dmat4(data.camera.projectionMatrix()) * modelViewTransform)
    );
    _program->setUniform(_uniformCache.aspectRatio, _aspectRatio);

    ghoul::opengl::TextureUnit filterTextureUnit;
    filterTextureUnit.activate();
    glBindTexture(GL_TEXTURE_2D, _filterTexture);
    defer{ glBindTexture(GL_TEXTURE_2D, 0); };

    _program->setUniform(_uniformCache.filterTexture, filterTextureUnit);

    // Drawing by indices:
    glBindVertexArray(_vao);
    glDrawElements(GL_TRIANGLES, _indicesArray.size(), GL_UNSIGNED_INT, (void *)0);
    glBindVertexArray(0);

    if (additiveBlending) {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(true);
    }

    _program->deactivate();

    checkGLErrors("after rendering");
}

void RenderableLines::update(const UpdateData&) {
    if (_program->isDirty()) {
        _program->rebuildFromFile();
        ghoul::opengl::updateUniformLocations(*_program, _uniformCache, UniformNames);
    }

    updateAspectRatio();

    if (_dataIsDirty) {
        updateGPUData();
        _dataIsDirty = false;
    }
}

void RenderableLines::updateAspectRatio() {
    const glm::vec2 dpiScaling = global::windowDelegate.dpiScaling();
    const glm::ivec2 res =
        glm::vec2(global::windowDelegate.currentDrawBufferResolution()) / dpiScaling;

    _aspectRatio = static_cast<float>(res.x) / static_cast<float>(res.y);
}

void RenderableLines::updateGPUData() {
    checkGLErrors("before update GPU data");
    if (_vao == 0u) {
        glGenVertexArrays(1, &_vao);
        glGenBuffers(1, &_vbo);
        glGenBuffers(1, &_ebo);
    }
        
    if (_verticesArray.empty() || _indicesArray.empty()) {
        return;
    }

    // Update vertex buffer
    glBindVertexArray(_vao);
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);

    glBufferData(
        GL_ARRAY_BUFFER, 
        _verticesArray.size() * sizeof(AAVertex), 
        _verticesArray.data(), 
        GL_STATIC_DRAW
    );

    // Update index buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ebo);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER, 
        _indicesArray.size() * sizeof(unsigned int),
        _indicesArray.data(), 
        GL_STATIC_DRAW
    );

    // p0
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(AAVertex),
        (void*)0
    );

    // p1
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(AAVertex),
        (void*)offsetof(AAVertex, _p1)
    );

    // weights
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(
        2,
        4,
        GL_FLOAT,
        GL_FALSE,
        sizeof(AAVertex),
        (void*)offsetof(AAVertex, _weights)
    );

    // radius
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(
        3,
        1,
        GL_FLOAT,
        GL_FALSE,
        sizeof(AAVertex),
        (void*)offsetof(AAVertex, _radius));

    glBindVertexArray(0);

    checkGLErrors("after update GPU data");
}

void RenderableLines::createFilterTexture() {
    // Allocate the filter texture.
    unsigned int* filterTextureData = new unsigned int[_filterTextureSize * _filterTextureSize];
    
    // Hermite interpolation (see The Renderman Companion - Upstill)
    auto smoothStep = [](float a, float b, float val) {
        float x = std::clamp((val - a) / (b - a), 0.0f, 1.0f);
        return x * x * (3.f - 2.f * x);
    };

    // Fill in the filter texture
    for (int i = 0; i < _filterTextureSize; i++)
    {
        for (int j = 0; j < _filterTextureSize; j++)
        {
            float t = sqrt(float(i*i) + float(j*j)) / float(_filterTextureSize);
            t = std::min(t, 1.0f);
            t = std::max(t, 0.0f);
            t = smoothStep(0.0f, 1.0f, t);
            unsigned int val = 255 - unsigned int(255.0f * t);

            /*
            // Test pattern.
            if( i==0 || (j==0 && i&1) )
                val = 255;
            else
                val = 50;
            */

            filterTextureData[i * _filterTextureSize + j] = 0x00ffffff + (val << 24);
        }
    }

        
    glGenTextures(1, &_filterTexture);
    //glGenFramebuffers(1, &_mainFramebuffer);

    glBindTexture(GL_TEXTURE_2D, _filterTexture);

    glTexImage2D(
        GL_TEXTURE_2D,
        0, // level
        GL_RED,
        _filterTextureSize,
        _filterTextureSize,
        0, // border
        GL_RED,
        GL_UNSIGNED_INT,
        filterTextureData
    );

    // JCC: No mipmapping can be used here.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    // JCC: Try other mirroring effects.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);

    checkGLErrors("after built filter texture");
}

void RenderableLines::addNewLine(
    const glm::vec3& p0, 
    const glm::vec3& p1, 
    float radius
) {
        
    _verticesArray.push_back(
        AAVertex(p0, p1, glm::vec4(1.0f, 0.0f, -1.0f, -1.0f), radius)
    );
    _verticesArray.push_back(
        AAVertex(p0, p1, glm::vec4(1.0f, 0.0f, -1.0f, 1.0f), radius)
    );
    _verticesArray.push_back(
        AAVertex(p0, p1, glm::vec4(1.0f, 0.0f, 0.0f, -1.0f), radius)
    );
    _verticesArray.push_back(
        AAVertex(p0, p1, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), radius)
    );
    _verticesArray.push_back(
        AAVertex(p0, p1, glm::vec4(0.0f, 1.0f, 0.0f, -1.0f), radius)
    );
    _verticesArray.push_back(
        AAVertex(p0, p1, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f), radius)
    );
    _verticesArray.push_back(
        AAVertex(p0, p1, glm::vec4(0.0f, 1.0f, 1.0f, -1.0f), radius)
    );
    _verticesArray.push_back(
        AAVertex(p0, p1, glm::vec4(0.0f, 1.0f, 1.0f, 1.0f), radius)
    );

    GLuint currentIndex = _indicesArray.size() / 18.f;
    _indicesArray.push_back(currentIndex * 8);
    _indicesArray.push_back(currentIndex * 8 + 2);
    _indicesArray.push_back(currentIndex * 8 + 3);
    _indicesArray.push_back(currentIndex * 8);
    _indicesArray.push_back(currentIndex * 8 + 3);
    _indicesArray.push_back(currentIndex * 8 + 1);

    _indicesArray.push_back(currentIndex * 8 + 2);
    _indicesArray.push_back(currentIndex * 8 + 4);
    _indicesArray.push_back(currentIndex * 8 + 5);
    _indicesArray.push_back(currentIndex * 8 + 2);
    _indicesArray.push_back(currentIndex * 8 + 5);
    _indicesArray.push_back(currentIndex * 8 + 3);

    _indicesArray.push_back(currentIndex * 8 + 4);
    _indicesArray.push_back(currentIndex * 8 + 6);
    _indicesArray.push_back(currentIndex * 8 + 7);
    _indicesArray.push_back(currentIndex * 8 + 4);
    _indicesArray.push_back(currentIndex * 8 + 7);
    _indicesArray.push_back(currentIndex * 8 + 5);

    updateGPUData();
    _dataIsDirty = false;
}

void RenderableLines::reset() {
    _dataIsDirty = true;
    _verticesArray.clear();
    _indicesArray.clear();
}

void RenderableLines::checkGLErrors(const std::string& identifier) const {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        switch (error) {
        case GL_INVALID_ENUM:
            LINFO(identifier + " - GL_INVALID_ENUM");
            break;
        case GL_INVALID_VALUE:
            LINFO(identifier + " - GL_INVALID_VALUE");
            break;
        case GL_INVALID_OPERATION:
            LINFO(identifier + " - GL_INVALID_OPERATION");
            break;
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            LINFO(identifier + " - GL_INVALID_FRAMEBUFFER_OPERATION");
            break;
        case GL_OUT_OF_MEMORY:
            LINFO(identifier + " - GL_OUT_OF_MEMORY");
            break;
        default:
            LINFO(identifier + " - Unknown error");
            break;
        }
    }
}
} // namespace openspace
