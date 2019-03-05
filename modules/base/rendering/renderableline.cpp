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

#include <modules/base/rendering/renderableline.h>

#include <modules/base/basemodule.h>
#include <openspace/documentation/documentation.h>
#include <openspace/documentation/verifier.h>
#include <openspace/engine/globals.h>
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
    constexpr const char* ProgramName = "AALine";

    enum BlendMode {
        BlendModeNormal = 0,
        BlendModeAdditive
    };

    constexpr openspace::properties::Property::PropertyInfo BillboardInfo = {
        "Billboard",
        "Billboard mode",
        "This value specifies whether the plane is a billboard, which means that it is "
        "always facing the camera. If this is false, it can be oriented using other "
        "transformations."
    };

    constexpr openspace::properties::Property::PropertyInfo SizeInfo = {
        "Size",
        "Size (in meters)",
        "This value specifies the size of the plane in meters."
    };

    constexpr openspace::properties::Property::PropertyInfo BlendModeInfo = {
        "BlendMode",
        "Blending Mode",
        "This determines the blending mode that is applied to this plane."
    };
} // namespace

namespace openspace {

    documentation::Documentation RenderableLine::Documentation() {
        using namespace documentation;
        return {
            "Renderable Line",
            "base_renderable_line",
            {
                {
                    SizeInfo.identifier,
                    new DoubleVerifier,
                    Optional::No,
                    SizeInfo.description
                },
                {
                    BillboardInfo.identifier,
                    new BoolVerifier,
                    Optional::Yes,
                    BillboardInfo.description
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

    RenderableLine::RenderableLine(const ghoul::Dictionary& dictionary)
        : Renderable(dictionary)
        , _billboard(BillboardInfo, false)
        , _size(SizeInfo, 10.f, 0.f, 1e25f)
        , _blendMode(BlendModeInfo, properties::OptionProperty::DisplayType::Dropdown)
    {
        documentation::testSpecificationAndThrow(
            Documentation(),
            dictionary,
            "RenderableLine"
        );

        addProperty(_opacity);
        registerUpdateRenderBinFromOpacity();

        _size = static_cast<float>(dictionary.value<double>(SizeInfo.identifier));

        if (dictionary.hasKey(BillboardInfo.identifier)) {
            _billboard = dictionary.value<bool>(BillboardInfo.identifier);
        }

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

        if (dictionary.hasKey(BlendModeInfo.identifier)) {
            const std::string v = dictionary.value<std::string>(BlendModeInfo.identifier);
            if (v == "Normal") {
                _blendMode = BlendModeNormal;
            }
            else if (v == "Additive") {
                _blendMode = BlendModeAdditive;
            }
        }

        addProperty(_billboard);

        addProperty(_size);
        _size.onChange([this]() { });

        setBoundingSphere(_size);
    }

    bool RenderableLine::isReady() const {
        return _shader != nullptr;
    }

    void RenderableLine::initializeGL() {
        glGenVertexArrays(1, &_quad); // generate array
        glGenBuffers(1, &_vertexPositionBuffer); // generate buffer
        
        _shader = BaseModule::ProgramObjectManager.request(
            ProgramName,
            []() -> std::unique_ptr<ghoul::opengl::ProgramObject> {
            return global::renderEngine.buildRenderProgram(
                ProgramName,
                absPath("${MODULE_BASE}/shaders/aaline_vs.glsl"),
                absPath("${MODULE_BASE}/shaders/aaline_fs.glsl")
            );
        }
        );

        createFilterTexture();
    }

    void RenderableLine::deinitializeGL() {
        glDeleteVertexArrays(1, &_quad);
        _quad = 0;

        glDeleteBuffers(1, &_vertexPositionBuffer);
        _vertexPositionBuffer = 0;

        BaseModule::ProgramObjectManager.release(
            ProgramName,
            [](ghoul::opengl::ProgramObject* p) {
            global::renderEngine.removeRenderProgram(p);
        }
        );
        _shader = nullptr;
    }

    void RenderableLine::render(const RenderData& data, RendererTasks&) {
        _shader->activate();

        _shader->setUniform("opacity", _opacity);

        const glm::dmat4 modelTransform =
            glm::translate(glm::dmat4(1.0), data.modelTransform.translation) *
            glm::dmat4(data.modelTransform.rotation) *
            glm::scale(glm::dmat4(1.0), glm::dvec3(data.modelTransform.scale)) *
            glm::dmat4(1.0);
        const glm::dmat4 modelViewTransform =
            data.camera.combinedViewMatrix() * modelTransform;

        _shader->setUniform("modelViewProjectionTransform",
            data.camera.projectionMatrix() * glm::mat4(modelViewTransform));

        _shader->setUniform("modelViewTransform", modelViewTransform);
            
        ghoul::opengl::TextureUnit unit;
        unit.activate();
        //bindTexture();
        //defer{ unbindTexture(); };

        //_shader->setUniform("texture1", unit);

        bool usingFramebufferRenderer = global::renderEngine.rendererImplementation() ==
            RenderEngine::RendererImplementation::Framebuffer;

        bool usingABufferRenderer = global::renderEngine.rendererImplementation() ==
            RenderEngine::RendererImplementation::ABuffer;

        if (usingABufferRenderer) {
            _shader->setUniform("additiveBlending", _blendMode == BlendModeAdditive);
        }

        bool additiveBlending = (_blendMode == BlendModeAdditive) && usingFramebufferRenderer;
        if (additiveBlending) {
            glDepthMask(false);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        }

        // Drawing by indices:
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _indexBuffer);
        glDrawElements(
            GL_TRIANGLES,
            _localIndices.size(),
            GL_UNSIGNED_INT,
            (void*)0
        );

        if (additiveBlending) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(true);
        }

        _shader->deactivate();
    }

    void RenderableLine::update(const UpdateData&) {
        if (_shader->isDirty()) {
            _shader->rebuildFromFile();
        }

    }

    /*
    void RenderableLine::createPlane() {
        const GLfloat size = _size;
        const GLfloat vertexData[] = {
            //      x      y     z     w     s     t
            -size, -size, 0.f, 0.f, 0.f, 0.f,
            size, size, 0.f, 0.f, 1.f, 1.f,
            -size, size, 0.f, 0.f, 0.f, 1.f,
            -size, -size, 0.f, 0.f, 0.f, 0.f,
            size, -size, 0.f, 0.f, 1.f, 0.f,
            size, size, 0.f, 0.f, 1.f, 1.f,
        };

        glBindVertexArray(_quad);
        glBindBuffer(GL_ARRAY_BUFFER, _vertexPositionBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertexData), vertexData, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 6, nullptr);

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(
            1,
            2,
            GL_FLOAT,
            GL_FALSE,
            sizeof(GLfloat) * 6,
            reinterpret_cast<void*>(sizeof(GLfloat) * 4)
        );
    }
    */

    void RenderableLine::createFilterTexture() {
        // Allocate the filter texture.
        unsigned int** filterTextureData = new unsigned int*[_filterTextureSize];
        for (int i = 0; i < _filterTextureSize; i++)
        {
            filterTextureData[i] = new unsigned int[_filterTextureSize];
        }

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

                filterTextureData[i][j] = 0x00ffffff + (val << 24);
            }
        }

        

        glGenTextures(1, &_filterTexture);
        //glGenFramebuffers(1, &_mainFramebuffer);

        glBindTexture(GL_TEXTURE_2D, _filterTexture);

        glTexImage2D(
            GL_TEXTURE_2D,
            0, // level
            GL_RGBA8,
            _filterTextureSize,
            _filterTextureSize,
            0, // border
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            filterTextureData
        );

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    }

    void RenderableLine::createTrianglesIndices() {
        // Fill in the local index buffer
        _localIndices.clear();
        _localIndices.reserve(MAXLINES);

        int index = 0;
        for (int i = 0; i < MAXLINES; i+= NUMBEROFINDICES)
        {
            _localIndices.push_back(i * 8);
            _localIndices.push_back(i * 8 + 2);
            _localIndices.push_back(i * 8 + 3);
            _localIndices.push_back(i * 8);
            _localIndices.push_back(i * 8 + 3);
            _localIndices.push_back(i * 8 + 1);

            _localIndices.push_back(i * 8 + 2);
            _localIndices.push_back(i * 8 + 4);
            _localIndices.push_back(i * 8 + 5);
            _localIndices.push_back(i * 8 + 2);
            _localIndices.push_back(i * 8 + 5);
            _localIndices.push_back(i * 8 + 3);

            _localIndices.push_back(i * 8 + 4);
            _localIndices.push_back(i * 8 + 6);
            _localIndices.push_back(i * 8 + 7);
            _localIndices.push_back(i * 8 + 4);
            _localIndices.push_back(i * 8 + 7);
            _localIndices.push_back(i * 8 + 5);
        }

        glGenBuffers(1, &_indexBuffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _indexBuffer);
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER, 
            _localIndices.size() * sizeof(unsigned int), 
            _localIndices.data(), 
            GL_STATIC_DRAW
        );

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

} // namespace openspace
