/****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014-2018                                                               *
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

#include <modules/space/rendering/renderablekeplerorbits.h>

#include <modules/base/basemodule.h>

#include <openspace/engine/openspaceengine.h>
#include <openspace/rendering/renderengine.h>
#include <openspace/documentation/verifier.h>
#include <openspace/util/time.h>
#include <openspace/util/updatestructures.h>

#include <ghoul/filesystem/filesystem.h>
#include <ghoul/misc/csvreader.h>

// Todo:
// Parse epoch correctly
// read distances using correct unit
// ...

namespace {
    constexpr const char* ProgramName = "KeplerTrails";
    
    static const openspace::properties::Property::PropertyInfo PathInfo = {
        "Path",
        "Path",
        "The file path to the CSV file to read"
    };
    
    static const openspace::properties::Property::PropertyInfo SegmentsInfo = {
        "Segments",
        "Segments",
        "The number of segments to use for each orbit ellipse"
    };
    
    static const openspace::properties::Property::PropertyInfo EccentricityColumnInfo = {
        "EccentricityColumn",
        "EccentricityColumn",
        "The header of the column where the eccentricity is stored"
    };
    
    static const openspace::properties::Property::PropertyInfo SemiMajorAxisColumnInfo = {
        "SemiMajorAxisColumn",
        "SemiMajorAxisColumn",
        "The header of the column where the semi-major axis is stored"
    };
    
    static const openspace::properties::Property::PropertyInfo SemiMajorAxisUnitInfo = {
        "SemiMajorAxisUnit",
        "SemiMajorAxisUnit",
        "The unit of the semi major axis. For example: If specified in km, set this to 1000."
    };
    
    static const openspace::properties::Property::PropertyInfo InclinationColumnInfo = {
        "InclinationColumn",
        "InclinationColumn",
        "The header of the column where the inclination is stored"
    };
    
    static const openspace::properties::Property::PropertyInfo AscendingNodeColumnInfo = {
        "AscendingNodeColumn",
        "AscendingNodeColumn",
        "The header of the column where the ascending node is stored"
    };
    
    static const openspace::properties::Property::PropertyInfo ArgumentOfPeriapsisColumnInfo = {
        "ArgumentOfPeriapsisColumn",
        "ArgumentOfPeriapsisColumn",
        "The header of the column where the argument of periapsis is stored"
    };
    
    static const openspace::properties::Property::PropertyInfo MeanAnomalyAtEpochColumnInfo = {
        "MeanAnomalyAtEpochColumn",
        "MeanAnomalyAtEpochColumn",
        "The header of the column where the mean anomaly at epoch is stored"
    };
    
    static const openspace::properties::Property::PropertyInfo EpochColumnInfo = {
        "EpochColumn",
        "EpochColumn",
        "The header of the column where the epoch is stored"
    };
}

namespace openspace {
 
documentation::Documentation RenderableKeplerOrbits::Documentation() {
    using namespace documentation;
    return {
        "Renderable Kepler Orbits",
        "space_renderable_kepler_orbits",
        {
            {
                SegmentsInfo.identifier,
                new DoubleVerifier,
                Optional::No,
                SegmentsInfo.description
            },
            {
                PathInfo.identifier,
                new StringVerifier,
                Optional::No,
                PathInfo.description
            },
            {
                EccentricityColumnInfo.identifier,
                new StringVerifier,
                Optional::No,
                EccentricityColumnInfo.description
            },
            {
                SemiMajorAxisColumnInfo.identifier,
                new StringVerifier,
                Optional::No,
                SemiMajorAxisColumnInfo.description
            },
            {
                SemiMajorAxisUnitInfo.identifier,
                new DoubleVerifier,
                Optional::No,
                SemiMajorAxisUnitInfo.description
            },
            {
                InclinationColumnInfo.identifier,
                new StringVerifier,
                Optional::No,
                InclinationColumnInfo.description
            },
            {
                AscendingNodeColumnInfo.identifier,
                new StringVerifier,
                Optional::No,
                AscendingNodeColumnInfo.description
            },
            {
                ArgumentOfPeriapsisColumnInfo.identifier,
                new StringVerifier,
                Optional::No,
                ArgumentOfPeriapsisColumnInfo.description
            },
            {
                MeanAnomalyAtEpochColumnInfo.identifier,
                new StringVerifier,
                Optional::No,
                MeanAnomalyAtEpochColumnInfo.description
            },
            {
                EpochColumnInfo.identifier,
                new StringVerifier,
                Optional::No,
                EpochColumnInfo.description
            }
        }
    };
}
    
RenderableKeplerOrbits::RenderableKeplerOrbits(const ghoul::Dictionary& dictionary)
    : Renderable(dictionary)
    , _path(PathInfo)
    , _nSegments(SegmentsInfo)
    , _eccentricityColumnName(EccentricityColumnInfo)
    , _semiMajorAxisColumnName(SemiMajorAxisColumnInfo)
    , _semiMajorAxisUnit(SemiMajorAxisUnitInfo)
    , _inclinationColumnName(InclinationColumnInfo)
    , _ascendingNodeColumnName(AscendingNodeColumnInfo)
    , _argumentOfPeriapsisColumnName(ArgumentOfPeriapsisColumnInfo)
    , _meanAnomalyAtEpochColumnName(MeanAnomalyAtEpochColumnInfo)
    , _epochColumnName(EpochColumnInfo)
{
    documentation::testSpecificationAndThrow(
         Documentation(),
         dictionary,
         "RenderableKeplerOrbits"
         );

    _nSegments =
        static_cast<int>(dictionary.value<double>(SegmentsInfo.identifier));
    _path =
        dictionary.value<std::string>(PathInfo.identifier);
    _eccentricityColumnName =
        dictionary.value<std::string>(EccentricityColumnInfo.identifier);
    _semiMajorAxisColumnName =
        dictionary.value<std::string>(SemiMajorAxisColumnInfo.identifier);
    _inclinationColumnName =
        dictionary.value<std::string>(InclinationColumnInfo.identifier);
    _ascendingNodeColumnName =
        dictionary.value<std::string>(AscendingNodeColumnInfo.identifier);
    _argumentOfPeriapsisColumnName =
        dictionary.value<std::string>(ArgumentOfPeriapsisColumnInfo.identifier);
    _meanAnomalyAtEpochColumnName =
        dictionary.value<std::string>(MeanAnomalyAtEpochColumnInfo.identifier);
    _epochColumnName =
        dictionary.value<std::string>(EpochColumnInfo.identifier);
    _semiMajorAxisUnit =
        dictionary.value<double>(SemiMajorAxisUnitInfo.identifier);
    
    addPropertySubOwner(_appearance);
    addProperty(_path);
    addProperty(_nSegments);
    addProperty(_semiMajorAxisUnit);
}

RenderableKeplerOrbits::~RenderableKeplerOrbits() {

}
    
void RenderableKeplerOrbits::initialize() {
    readFromCsvFile();
    updateBuffers();

    _path.onChange([this]() {
        readFromCsvFile();
        updateBuffers();
    });
    
    _semiMajorAxisUnit.onChange([this]() {
        readFromCsvFile();
        updateBuffers();
    });

    _nSegments.onChange([this]() {
        updateBuffers();
    });
}

void RenderableKeplerOrbits::deinitialize() {
    
}
 
void RenderableKeplerOrbits::initializeGL() {
    glGenVertexArrays(1, &_vertexArray);
    glGenBuffers(1, &_vertexBuffer);
    glGenBuffers(1, &_indexBuffer);
    
    _programObject = BaseModule::ProgramObjectManager.requestProgramObject(
       ProgramName,
       []() -> std::unique_ptr<ghoul::opengl::ProgramObject> {
           return OsEng.renderEngine().buildRenderProgram(
               ProgramName,
               absPath("${MODULE_SPACE}/shaders/renderablekeplerorbits_vs.glsl"),
               absPath("${MODULE_SPACE}/shaders/renderablekeplerorbits_fs.glsl")
           );
       }
   );
    
    _uniformCache.opacity = _programObject->uniformLocation("opacity");
    _uniformCache.modelView = _programObject->uniformLocation("modelViewTransform");
    _uniformCache.projection = _programObject->uniformLocation("projectionTransform");
    _uniformCache.color = _programObject->uniformLocation("color");
    _uniformCache.useLineFade = _programObject->uniformLocation("useLineFade");
    _uniformCache.lineFade = _programObject->uniformLocation("lineFade");
    
    setRenderBin(Renderable::RenderBin::Overlay);
}
    
void RenderableKeplerOrbits::deinitializeGL() {
    BaseModule::ProgramObjectManager.releaseProgramObject(ProgramName);
    
    glDeleteBuffers(1, &_vertexBuffer);
    glDeleteBuffers(1, &_indexBuffer);
    glDeleteVertexArrays(1, &_vertexArray);
}

    
bool RenderableKeplerOrbits::isReady() const {
    return true;
}

void RenderableKeplerOrbits::update(const UpdateData&) {}
    
void RenderableKeplerOrbits::render(const RenderData& data, RendererTasks&) {
    _programObject->activate();
    _programObject->setUniform(_uniformCache.opacity, _opacity);
    
    glm::dmat4 modelTransform =
        glm::translate(glm::dmat4(1.0), data.modelTransform.translation) *
        glm::dmat4(data.modelTransform.rotation) *
        glm::scale(glm::dmat4(1.0), glm::dvec3(data.modelTransform.scale));

    _programObject->setUniform(
        _uniformCache.modelView,
        data.camera.combinedViewMatrix() * modelTransform
    );

    _programObject->setUniform(_uniformCache.projection, data.camera.projectionMatrix());
    _programObject->setUniform(_uniformCache.color, _appearance.lineColor);
    //_programObject->setUniform(_uniformCache.useLineFade, _appearance.useLineFade);

    /*if (_appearance.useLineFade) {
        _programObject->setUniform(_uniformCache.lineFade, _appearance.lineFade);
    }*/

    glDepthMask(false);
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    glBindVertexArray(_vertexArray);
    glDrawElements(GL_LINES,
                   static_cast<unsigned int>(_indexBufferData.size()),
                   GL_UNSIGNED_INT,
                   0);
    glBindVertexArray(0);
    _programObject->deactivate();
}

void RenderableKeplerOrbits::updateBuffers() {
    const size_t nVerticesPerOrbit = _nSegments + 1;
    _vertexBufferData.resize(_orbits.size() * nVerticesPerOrbit);
    _indexBufferData.resize(_orbits.size() * _nSegments * 2);
    
    size_t orbitIndex = 0;
    size_t elementIndex = 0;
    for (const auto& orbit : _orbits) {
        KeplerTranslation keplerTranslation(orbit);
        const double period = orbit.period();
        for (size_t i = 0; i <= _nSegments; ++i) {
            size_t index = orbitIndex * nVerticesPerOrbit + i;

            double timeOffset = period *
                static_cast<float>(i) / static_cast<float>(_nSegments);
            glm::vec3 position =
                keplerTranslation.position(Time(orbit.epoch + timeOffset));

            _vertexBufferData[index].x = position.x;
            _vertexBufferData[index].y = position.y;
            _vertexBufferData[index].z = position.z;
            _vertexBufferData[index].time = timeOffset;
            if (i > 0) {
                _indexBufferData[elementIndex++] = static_cast<unsigned int>(index) - 1;
                _indexBufferData[elementIndex++] = static_cast<unsigned int>(index);
            }
        }
        ++orbitIndex;
    }
    
    glBindVertexArray(_vertexArray);
    
    glBindBuffer(GL_ARRAY_BUFFER, _vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER,
                 _vertexBufferData.size() * sizeof(TrailVBOLayout),
                 _vertexBufferData.data(),
                 GL_STATIC_DRAW
                 );
    

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 _indexBufferData.size() * sizeof(int),
                 _indexBufferData.data(),
                 GL_STATIC_DRAW
                 );
    
    glBindVertexArray(0);
}

void RenderableKeplerOrbits::readFromCsvFile() {
    std::vector<std::string> columns = {
        _eccentricityColumnName,
        _semiMajorAxisColumnName,
        _inclinationColumnName,
        _ascendingNodeColumnName,
        _argumentOfPeriapsisColumnName,
        _meanAnomalyAtEpochColumnName,
        _epochColumnName,
    };
    
    std::vector<std::vector<std::string>> data =
        ghoul::loadCSVFile(_path, columns, false);

    _orbits.resize(data.size());
    
    size_t i = 0;
    for (const std::vector<std::string>& line : data) {
        _orbits[i++] = KeplerTranslation::KeplerOrbit{
            std::stof(line[0]),
            _semiMajorAxisUnit * std::stof(line[1]) / 1000.0,
            std::stof(line[2]),
            std::stof(line[3]),
            std::stof(line[4]),
            std::stof(line[5]),
            std::stof(line[6])
        };
    }
}
    
}
