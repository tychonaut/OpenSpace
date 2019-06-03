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
    : RenderableLines(dictionary)
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

    addProperty(_lineWidth);
    //_lineWidth.onChange([this]() {});

    addProperty(_currentLineColor);
    _lineWidth.onChange([this]() {
        this->_lineColor = _currentLineColor;
    }
    );
}


//void RenderableEarthMoonLine::initializeGL() {
//}
//
//void RenderableEarthMoonLine::deinitializeGL() {
//    // Delete line
//    this->reset();
//}

//void RenderableEarthMoonLine::render(const RenderData& data, RendererTasks&) {    
//    this->render();
//}

void RenderableEarthMoonLine::update(const UpdateData& ud) {
    
    // Clean previous line
    reset();
    
    // JCC: add Line
    double time = openspace::Time::now().j2000Seconds();
    double lt;
    glm::dvec3 EarthPosWorld = SpiceManager::ref().targetPosition(
        "EARTH",
        "SUN",
        "GALACTIC",
        {},
        time,
        lt
    );

    glm::dvec3 MoonPosWorld = SpiceManager::ref().targetPosition(
        "MOON",
        "SUN",
        "GALACTIC",
        {},
        time,
        lt
    );

    const glm::dmat4 modelTransform =
        glm::translate(glm::dmat4(1.0), ud.modelTransform.translation) *
        glm::dmat4(ud.modelTransform.rotation) *
        glm::scale(glm::dmat4(1.0), glm::dvec3(ud.modelTransform.scale)) *
        glm::dmat4(1.0);
    const glm::dmat4 worldToModelTransform = glm::inverse(modelTransform);

    glm::vec3 earthP = glm::vec3(worldToModelTransform * glm::dvec4(EarthPosWorld, 1.0));
    glm::vec3 moonP = glm::vec3(worldToModelTransform * glm::dvec4(MoonPosWorld, 1.0));

    addNewLine(earthP, moonP, _lineWidth);
}

} // namespace openspace
