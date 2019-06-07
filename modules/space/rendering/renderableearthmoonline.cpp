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

#include <modules/space/rendering/renderableearthmoonline.h>

#include <openspace/documentation/documentation.h>
#include <openspace/documentation/verifier.h>
#include <openspace/rendering/renderengine.h>
#include <openspace/util/spicemanager.h>
#include <openspace/util/time.h>
#include <openspace/util/updatestructures.h>

#include <ghoul/filesystem/filesystem.h>
#include <ghoul/misc/defer.h>
#include <ghoul/opengl/programobject.h>
#include <ghoul/glm.h>

#include <glm/gtx/string_cast.hpp>

namespace {
    constexpr const char* ProgramName = "EarthMoonAALine";

    constexpr openspace::properties::Property::PropertyInfo LineWidthInfo = {
        "LineWidth",
        "Line width (in pixels)",
        "This value specifies the width in pixels to be used."
    };

    constexpr openspace::properties::Property::PropertyInfo LineColorInfo = {
        "LineColor",
        "Line color",
        "This value specifies the color of the line to be used."
    };

} // namespace

namespace openspace {

documentation::Documentation RenderableEarthMoonLine::Documentation() {
    using namespace documentation;
    return {
        "Renderable Line",
        "base_renderable_line",
        {
            {
                LineWidthInfo.identifier,
                new DoubleVerifier,
                Optional::Yes,
                LineWidthInfo.description
            },
            {
                LineColorInfo.identifier,
                new Vector4Verifier<float>,
                Optional::Yes,
                LineColorInfo.description,
            }
        }
    };
}

RenderableEarthMoonLine::RenderableEarthMoonLine(const ghoul::Dictionary& dictionary)
    : Renderable(dictionary)
    , _renderableLines(dictionary)
    , _lineWidth(LineWidthInfo, 10.f, 1.f, 200.f)
    , _currentLineColor(
        LineColorInfo,
        glm::vec4(1.f, 1.f, 1.f, 1.f),
        glm::vec4(0.f, 0.f, 0.f, 0.f),
        glm::vec4(1.f, 1.f, 1.f, 1.f)
    )
{
    documentation::testSpecificationAndThrow(
        Documentation(),
        dictionary,
        "RenderableEarthMoonLine"
    );

    if (dictionary.hasKey(LineWidthInfo.identifier)) {
        _lineWidth = dictionary.value<float>(LineWidthInfo.identifier);
    }

    if (dictionary.hasKey(LineColorInfo.identifier)) {
        _currentLineColor = dictionary.value<glm::vec4>(LineColorInfo.identifier);
    }

    // JCC TMP:
    auto drawnTank = [this]() {
        _renderableLines.reset();
        // ground
        glm::vec3 p00(-9, 6, 0);
        glm::vec3 p01(8, 6, 0);
        glm::vec3 p02(8, -6, 0);
        glm::vec3 p03(-9, -6, 0);

        // beltline
        glm::vec3 p04(-11, 8, 3);
        glm::vec3 p05(11, 8, 3);
        glm::vec3 p06(11, -8, 3);
        glm::vec3 p07(-11, -8, 3);

        // turret bottom
        glm::vec3 p08(-8, 4, 5);
        glm::vec3 p09(3, 4, 5);
        glm::vec3 p10(3, -4, 5);
        glm::vec3 p11(-8, -4, 5);

        // turret peak
        glm::vec3 p12(-6, 2, 8);
        glm::vec3 p13(-6, -2, 8);

        // gun
        //   muzzle
        glm::vec3 p14(10, 0.5, 6);
        glm::vec3 p15(10, -0.5, 6);
        glm::vec3 p16(10, -0.5, 7);
        glm::vec3 p17(10, 0.5, 7);
        //   base
        glm::vec3 p18(0, 0.5, 6);
        glm::vec3 p19(0, -0.5, 6);
        glm::vec3 p20(-3, -0.5, 7);
        glm::vec3 p21(-3, 0.5, 7);

        // radar
        glm::vec3 p22(-6, 0, 8);
        glm::vec3 p23(-6, -1, 8.5);
        glm::vec3 p24(-5.5, -2, 9);
        glm::vec3 p25(-5.5, -2, 9.5);
        glm::vec3 p26(-6, -1, 10);
        glm::vec3 p27(-6, 1, 10);
        glm::vec3 p28(-5.5, 2, 9.5);
        glm::vec3 p29(-5.5, 2, 9);
        glm::vec3 p30(-6, 1, 8.5);
        glm::vec3 p31(-6, 0, 8.5);


        // 3 bands, bottom up
        _renderableLines.addNewLine(p00, p01, _lineWidth);
        _renderableLines.addNewLine(p01, p02, _lineWidth);
        _renderableLines.addNewLine(p02, p03, _lineWidth);
        _renderableLines.addNewLine(p03, p00, _lineWidth);

        _renderableLines.addNewLine(p04, p05, _lineWidth);
        _renderableLines.addNewLine(p05, p06, _lineWidth);
        _renderableLines.addNewLine(p06, p07, _lineWidth);
        _renderableLines.addNewLine(p07, p04, _lineWidth);

        _renderableLines.addNewLine(p08, p09, _lineWidth);
        _renderableLines.addNewLine(p09, p10, _lineWidth);
        _renderableLines.addNewLine(p10, p11, _lineWidth);
        _renderableLines.addNewLine(p11, p08, _lineWidth);

        // vertical joins for bottom
        _renderableLines.addNewLine(p00, p04, _lineWidth);
        _renderableLines.addNewLine(p01, p05, _lineWidth);
        _renderableLines.addNewLine(p02, p06, _lineWidth);
        _renderableLines.addNewLine(p03, p07, _lineWidth);

        _renderableLines.addNewLine(p08, p04, _lineWidth);
        _renderableLines.addNewLine(p09, p05, _lineWidth);
        _renderableLines.addNewLine(p10, p06, _lineWidth);
        _renderableLines.addNewLine(p11, p07, _lineWidth);

        // turret
        _renderableLines.addNewLine(p08, p12, _lineWidth);
        _renderableLines.addNewLine(p13, p12, _lineWidth);
        _renderableLines.addNewLine(p11, p13, _lineWidth);
        _renderableLines.addNewLine(p10, p13, _lineWidth);
        _renderableLines.addNewLine(p09, p12, _lineWidth);

        // gun
        _renderableLines.addNewLine(p14, p15, _lineWidth);
        _renderableLines.addNewLine(p15, p16, _lineWidth);
        _renderableLines.addNewLine(p16, p17, _lineWidth);
        _renderableLines.addNewLine(p17, p14, _lineWidth);

        _renderableLines.addNewLine(p18, p19, _lineWidth);
        _renderableLines.addNewLine(p19, p20, _lineWidth);
        _renderableLines.addNewLine(p20, p21, _lineWidth);
        _renderableLines.addNewLine(p21, p18, _lineWidth);

        _renderableLines.addNewLine(p14, p18, _lineWidth);
        _renderableLines.addNewLine(p15, p19, _lineWidth);
        _renderableLines.addNewLine(p16, p20, _lineWidth);
        _renderableLines.addNewLine(p17, p21, _lineWidth);

        // radar
        _renderableLines.addNewLine(p23, p24, _lineWidth);
        _renderableLines.addNewLine(p24, p25, _lineWidth);
        _renderableLines.addNewLine(p25, p26, _lineWidth);
        _renderableLines.addNewLine(p26, p27, _lineWidth);
        _renderableLines.addNewLine(p27, p28, _lineWidth);
        _renderableLines.addNewLine(p28, p29, _lineWidth);
        _renderableLines.addNewLine(p29, p30, _lineWidth);
        _renderableLines.addNewLine(p30, p23, _lineWidth);

        _renderableLines.addNewLine(p23, p26, _lineWidth);
        _renderableLines.addNewLine(p27, p30, _lineWidth);

        _renderableLines.addNewLine(p22, p31, _lineWidth);
    };

    addPropertySubOwner(_renderableLines);

    addProperty(_lineWidth);
    _lineWidth.onChange([this, drawnTank]() {
        drawnTank();
    }
    );

    addProperty(_currentLineColor);
    _currentLineColor.onChange([this]() {
        _renderableLines.setLineColor(_currentLineColor);
        }
    );
    _renderableLines.setLineColor(_currentLineColor);

    //_renderableLines.setGPUMemoryAccessType(GL_STATIC_DRAW);
    //drawnTank();
}


void RenderableEarthMoonLine::initializeGL() {
    _renderableLines.initializeGL();
}

void RenderableEarthMoonLine::deinitializeGL() {
    _renderableLines.reset();
    _renderableLines.deinitializeGL();
}

void RenderableEarthMoonLine::render(const RenderData& data, RendererTasks&) {    
    _renderableLines.render(data);
}

void RenderableEarthMoonLine::update(const UpdateData& data) {
    
    // Clean previous line
    _renderableLines.reset();
    _renderableLines.setGPUMemoryAccessType(GL_DYNAMIC_DRAW);
    
    // JCC: add Line
    double time = data.time.j2000Seconds();
    double lt;
    glm::dvec3 EarthPosWorld = SpiceManager::ref().targetPosition(
        "EARTH",
        "SUN",
        "GALACTIC",
        {},
        time,
        lt
    );
    EarthPosWorld *= 1000.0; // Km to meters
    
    glm::dvec3 MoonPosWorld = SpiceManager::ref().targetPosition(
        "MOON",
        "SUN",
        "GALACTIC",
        {},
        time,
        lt
    );
    MoonPosWorld *= 1000.0; // Km to meters

    const glm::dmat4 modelTransform =
        glm::translate(glm::dmat4(1.0), data.modelTransform.translation) *
        glm::dmat4(data.modelTransform.rotation) *
        glm::scale(glm::dmat4(1.0), glm::dvec3(data.modelTransform.scale));
    const glm::dmat4 worldToModelTransform = glm::inverse(modelTransform);

    glm::vec3 earthP = glm::vec3(worldToModelTransform * glm::dvec4(EarthPosWorld, 1.0));
    glm::vec3 moonP = glm::vec3(worldToModelTransform * glm::dvec4(MoonPosWorld, 1.0));

    _renderableLines.addNewLine(earthP, moonP, _lineWidth);

    _renderableLines.update();
}

bool RenderableEarthMoonLine::isReady() const {
    return true;
}

} // namespace openspace
