/*****************************************************************************************
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

#include <modules/globebrowsing/util/globelabelscomponent.h>
#include <modules/globebrowsing/globebrowsingmodule.h>
#include <modules/globebrowsing/globes/renderableglobe.h>

#include <openspace/documentation/documentation.h>
#include <openspace/documentation/verifier.h>
#include <openspace/util/updatestructures.h>
#include <openspace/engine/openspaceengine.h>
#include <openspace/engine/moduleengine.h>

#include <ghoul/filesystem/cachemanager.h>
#include <ghoul/filesystem/filesystem.h>
#include <ghoul/logging/logmanager.h>

#include <ghoul/filesystem/filesystem.h>
#include <ghoul/misc/dictionary.h>
#include <ghoul/opengl/programobject.h>

#include <ghoul/font/fontmanager.h>
#include <ghoul/font/fontrenderer.h>

#include <fstream>
#include <cstdlib>
#include <locale>

namespace {
    constexpr const char* keyLabels = "Labels";
    constexpr const char* keyLabelsFileName = "FileName";

    constexpr const char* _loggerCat = "GlobeLabels";

    constexpr int8_t CurrentCacheVersion = 1;

    static const openspace::properties::Property::PropertyInfo LabelsInfo = {
        "Labels",
        "Labels Enabled",
        "Enables and disables the rendering of labels on the globe surface from "
        "the csv label file"
    };

    static const openspace::properties::Property::PropertyInfo LabelsFontSizeInfo = {
        "LabelsFontSize",
        "Labels Font Size",
        "Font size for the rendering labels. This is different fromt text size."
    };

    static const openspace::properties::Property::PropertyInfo LabelsMaxSizeInfo = {
        "LabelsMaxSize",
        "Labels Maximum Text Size",
        "Maximum label size"
    };

    static const openspace::properties::Property::PropertyInfo LabelsMinSizeInfo = {
        "LabelsMinSize",
        "Labels Minimum Text Size",
        "Minimum label size"
    };

    static const openspace::properties::Property::PropertyInfo LabelsSizeInfo = {
        "LabelsSize",
        "Labels Size",
        "Labels Size"
    };

    static const openspace::properties::Property::PropertyInfo LabelsMinHeightInfo = {
        "LabelsMinHeight",
        "Labels Minimum Height",
        "Labels Minimum Height"
    };

    static const openspace::properties::Property::PropertyInfo LabelsColorInfo = {
        "LabelsColor",
        "Labels Color",
        "Labels Color"
    };

    static const openspace::properties::Property::PropertyInfo FadeInStartingDistanceInfo = {
        "FadeInStartingDistance",
        "Fade In Starting Distance for Labels",
        "Fade In Starting Distance for Labels"
    };

    static const openspace::properties::Property::PropertyInfo LabelsFadeInEnabledInfo = {
        "LabelsFadeInEnabled",
        "Labels fade In enabled",
        "Labels fade In enabled"
    };
} // namespace

namespace openspace {

    documentation::Documentation GlobeLabelsComponent::Documentation() {
        using namespace documentation;
        return {
            "GlobeLabels Component",
            "globebrowsing_globelabelscomponent",
        {
            {
                LabelsInfo.identifier,
                new BoolVerifier,
                Optional::No,
                LabelsInfo.description
            },
            {
                LabelsFontSizeInfo.identifier,
                new IntVerifier,
                Optional::No,
                LabelsFontSizeInfo.description
            },
            {
                LabelsMaxSizeInfo.identifier,
                new IntVerifier,
                Optional::Yes,
                LabelsMaxSizeInfo.description
            },
            {
                LabelsMinSizeInfo.identifier,
                new IntVerifier,
                Optional::Yes,
                LabelsMinSizeInfo.description
            },
            {
                LabelsSizeInfo.identifier,
                new DoubleVerifier,
                Optional::Yes,
                LabelsSizeInfo.description
            },
            {
                LabelsMinHeightInfo.identifier,
                new DoubleVerifier,
                Optional::Yes,
                LabelsMinHeightInfo.description
            },
            {
                LabelsColorInfo.identifier,
                new Vector4Verifier<float>(),
                Optional::Yes,
                LabelsColorInfo.description
            },
            {
                FadeInStartingDistanceInfo.identifier,
                new DoubleVerifier,
                Optional::Yes,
                FadeInStartingDistanceInfo.description
            },
            {
                LabelsFadeInEnabledInfo.identifier,
                new BoolVerifier,
                Optional::Yes,
                LabelsFadeInEnabledInfo.description
            },
        }
        };
    }

    GlobeLabelsComponent::GlobeLabelsComponent()
        : properties::PropertyOwner({ "GlobeLabelsComponent" })
        , _labelsEnabled(LabelsInfo, false)
        , _labelsFontSize(LabelsFontSizeInfo, 30, 1, 120)
        , _labelsMaxSize(LabelsMaxSizeInfo, 300, 10, 1000)
        , _labelsMinSize(LabelsMinSizeInfo, 4, 1, 100)
        , _labelsSize(LabelsSizeInfo, 2.5, 0, 30)
        , _labelsMinHeight(LabelsMinHeightInfo, 100.0, 0.0, 10000.0)
        , _labelsColor(LabelsColorInfo, glm::vec4(1.f, 1.f, 0.f, 1.f),
            glm::vec4(0.f), glm::vec4(1.f))
        , _labelsFadeInDist(FadeInStartingDistanceInfo, 1E6, 1E3, 1E8)
        , _labelsFadeInEnabled(LabelsFadeInEnabledInfo, true)
        , _labelsDataPresent(false)
    {
        addProperty(_labelsEnabled);
        addProperty(_labelsFontSize);
        addProperty(_labelsSize);
        addProperty(_labelsMinHeight);
        _labelsColor.setViewOption(properties::Property::ViewOptions::Color);
        addProperty(_labelsColor);
        addProperty(_labelsFadeInDist);
        addProperty(_labelsMinSize);
        addProperty(_labelsFadeInEnabled);

        //_applyTextureSize.onChange([this]() { ; });
    }

    void GlobeLabelsComponent::initialize(const ghoul::Dictionary& dictionary, 
        globebrowsing::RenderableGlobe* globe, std::shared_ptr<ghoul::fontrendering::Font> font)
    {
        documentation::testSpecificationAndThrow(
            Documentation(),
            dictionary,
            "GlobeLabelsComponent"
        );
        
        _globe = globe;

        // Reads labels' file and build cache file if necessary
        _labelsDataPresent = false;
        ghoul::Dictionary labelsDictionary;
        bool successLabels = dictionary.getValue(keyLabels, labelsDictionary);
        if (successLabels) {
            std::string labelsFile;
            successLabels = labelsDictionary.getValue(keyLabelsFileName, labelsFile);
            // DEBUG:
            //std::cout << "========== File Name: " << absPath(labelsFile) << " ===========" << std::endl;
            if (successLabels) {
                _labelsDataPresent = true;
                bool loadSuccess = loadLabelsData(absPath(labelsFile));
                if (loadSuccess) {
                    _labelsEnabled.set(true);
                    //_chunkedLodGlobe->setLabels(_labels);
                    //_chunkedLodGlobe->enableLabelsRendering(true);

                    _labelsEnabled.onChange([&]() {
                        //_chunkedLodGlobe->enableLabelsRendering(_labelsEnabled);
                    });

                    _labelsFontSize.onChange([&]() {
                        //_chunkedLodGlobe->setFontSize(_labelsFontSize);
                    });

                    if (labelsDictionary.hasKey(LabelsSizeInfo.identifier)) {
                        float size = static_cast<float>(
                            labelsDictionary.value<double>(LabelsSizeInfo.identifier)
                            );
                        //_chunkedLodGlobe->setLabelsSize(size);
                        _labelsSize.set(size);
                    }

                    _labelsSize.onChange([&]() {
                        //_chunkedLodGlobe->setLabelsSize(_labelsSize);
                    });

                    if (labelsDictionary.hasKey(LabelsMinHeightInfo.identifier)) {
                        float height = labelsDictionary.value<float>(LabelsMinHeightInfo.identifier);
                        //_chunkedLodGlobe->setLabelsMinHeight(height);
                        _labelsMinHeight.set(height);
                    }

                    _labelsMinHeight.onChange([&]() {
                        //_chunkedLodGlobe->setLabelsMinHeight(_labelsMinHeight);
                    });

                    /*_labelsMaxHeight.onChange([&]() {
                    _chunkedLodGlobe->setLabelsMaxHeight(_labelsMaxHeight);
                    });*/

                    if (labelsDictionary.hasKey(LabelsColorInfo.identifier)) {
                        _labelsColor = labelsDictionary.value<glm::vec4>(
                            LabelsColorInfo.identifier
                            );
                        //_chunkedLodGlobe->setLabelsColor(_labelsColor);
                    }

                    _labelsColor.onChange([&]() {
                        //_labelsColor = _labelsColor;
                        //_chunkedLodGlobe->setLabelsColor(_labelsColor);
                    });

                    if (labelsDictionary.hasKey(FadeInStartingDistanceInfo.identifier)) {
                        float dist = labelsDictionary.value<float>(
                            FadeInStartingDistanceInfo.identifier
                            );
                        //_chunkedLodGlobe->setLabelFadeInDistance(dist);
                        _labelsFadeInDist.set(dist);
                    }

                    _labelsFadeInDist.onChange([&]() {
                        //_chunkedLodGlobe->setLabelFadeInDistance(
                        //    _labelsFadeInDist
                        //);
                    });

                    if (labelsDictionary.hasKey(LabelsMinSizeInfo.identifier)) {
                        int size = labelsDictionary.value<int>(LabelsMinSizeInfo.identifier);
                        //_chunkedLodGlobe->setLabelsMinSize(size);
                        _labelsMinSize.set(size);
                    }

                    _labelsMinSize.onChange([&]() {
                        //_chunkedLodGlobe->setLabelsMinSize(_labelsMinSize);
                    });

                    if (labelsDictionary.hasKey(LabelsFadeInEnabledInfo.identifier)) {
                        bool enabled = labelsDictionary.value<bool>(
                            LabelsFadeInEnabledInfo.identifier
                            );
                        //_chunkedLodGlobe->enableLabelsFadeIn(enabled);
                        _labelsFadeInEnabled.set(enabled);
                    }

                    _labelsFadeInEnabled.onChange([&]() {
                        //_chunkedLodGlobe->enableLabelsFadeIn(
                        //    _labelsFadeInEnabled
                        //);
                    });

                    _font = font;
                    initializeFonts();
                }
            }
        }
    }

    bool GlobeLabelsComponent::initializeGL() {
        return true;
    }

    void GlobeLabelsComponent::initializeFonts() {
        if (_font == nullptr) {
            _font = OsEng.fontManager().font(
                "Mono",
                static_cast<float>(_labelsFontSize),
                ghoul::fontrendering::FontManager::Outline::Yes,
                ghoul::fontrendering::FontManager::LoadGlyphs::No
            );
        }
    }


    bool GlobeLabelsComponent::deinitialize() {
        return true;
    }

    bool GlobeLabelsComponent::isReady() const {
        return true;
    }

    void GlobeLabelsComponent::update() {
        
    }

    bool GlobeLabelsComponent::loadLabelsData(const std::string& file) {
        bool success = true;
        if (_labelsDataPresent) {
            std::string cachedFile = FileSys.cacheManager()->cachedFilename(
                ghoul::filesystem::File(file),
                "GlobeLabelsComponent|" + identifier(),
                ghoul::filesystem::CacheManager::Persistent::Yes
            );

            bool hasCachedFile = FileSys.fileExists(cachedFile);
            if (hasCachedFile) {
                LINFO(fmt::format(
                    "Cached file '{}' used for labels file '{}'",
                    cachedFile,
                    file
                ));

                success = loadCachedFile(cachedFile);
                if (success) {
                    return true;
                }
                else {
                    FileSys.cacheManager()->removeCacheFile(file);
                    // Intentional fall-through to the 'else' to generate the cache
                    // file for the next run
                }
            }
            else {
                LINFO(fmt::format("Cache for labels file '{}' not found", file));
            }
            LINFO(fmt::format("Loading labels file '{}'", file));

            success = readLabelsFile(file);
            if (!success) {
                return false;
            }

            success &= saveCachedFile(cachedFile);
        }
        return success;
    }

    bool GlobeLabelsComponent::readLabelsFile(const std::string& file) {
        try {
            std::fstream csvLabelFile(file);
            if (!csvLabelFile.good()) {
                LERROR(fmt::format("Failed to open labels file '{}'", file));
                return false;
            }
            if (csvLabelFile.is_open()) {
                char line[4096];
                _labels.labelsArray.clear();
                while (!csvLabelFile.eof()) {
                    csvLabelFile.getline(line, 4090);
                    if (strnlen(line, 4090) > 10) {
                        LabelEntry lEntry;
                        char *token = strtok(line, ",");
                        // First line is just the Header
                        if (strcmp(token, "Feature_Name") == 0) {
                            continue;
                        }
                        strncpy(lEntry.feature, token, 256);
                        // Removing non ASCII characters:
                        int tokenChar = 0;
                        while (tokenChar < 256) {
                            if ((lEntry.feature[tokenChar] < 0 ||
                                lEntry.feature[tokenChar] > 127) &&
                                lEntry.feature[tokenChar] != '\0') {
                                lEntry.feature[tokenChar] = '*';
                            }
                            else if (lEntry.feature[tokenChar] == '\"') {
                                lEntry.feature[tokenChar] = '=';
                            }
                            tokenChar++;
                        }

                        strtok(NULL, ","); // Target is not used
                        lEntry.diameter = static_cast<float>(atof(strtok(NULL, ",")));
                        lEntry.latitude = static_cast<float>(atof(strtok(NULL, ",")));
                        lEntry.longitude = static_cast<float>(atof(strtok(NULL, ",")));
                        char * coordinateSystem = strtok(NULL, ",");

                        if (strstr(coordinateSystem, "West") != NULL) {
                            lEntry.longitude = 360.0f - lEntry.longitude;
                        }

                        // Clean white spaces
                        strncpy(lEntry.feature, strtok(lEntry.feature, "="), 256);

                        GlobeBrowsingModule* _globeBrowsingModule =
                            OsEng.moduleEngine().module<openspace::GlobeBrowsingModule>();
                        lEntry.geoPosition = _globeBrowsingModule->cartesianCoordinatesFromGeo(
                            *_globe,
                            lEntry.latitude,
                            lEntry.longitude,
                            lEntry.diameter
                        );

                        _labels.labelsArray.push_back(lEntry);
                    }
                }
                return true;
            }
            else {
                return false;
            }
        }
        catch (const std::fstream::failure& e) {
            LERROR(fmt::format("Failed reading labels file '{}'", file));
            LERROR(e.what());
            return false;
        }
    }

    bool GlobeLabelsComponent::loadCachedFile(const std::string& file) {
        std::ifstream fileStream(file, std::ifstream::binary);
        if (fileStream.good()) {
            int8_t version = 0;
            fileStream.read(reinterpret_cast<char*>(&version), sizeof(int8_t));
            if (version != CurrentCacheVersion) {
                LINFO("The format of the cached file has changed: deleting old cache");
                fileStream.close();
                FileSys.deleteFile(file);
                return false;
            }

            int32_t nValues = 0;
            fileStream.read(reinterpret_cast<char*>(&nValues), sizeof(int32_t));
            _labels.labelsArray.resize(nValues);

            fileStream.read(reinterpret_cast<char*>(&_labels.labelsArray[0]),
                nValues * sizeof(_labels.labelsArray[0]));

            bool success = fileStream.good();
            return success;
        }
        else {
            LERROR(fmt::format("Error opening file '{}' for loading cache file", file));
            return false;
        }
    }

    bool GlobeLabelsComponent::saveCachedFile(const std::string& file) const {

        std::ofstream fileStream(file, std::ofstream::binary);
        if (fileStream.good()) {
            fileStream.write(reinterpret_cast<const char*>(&CurrentCacheVersion),
                sizeof(int8_t));

            int32_t nValues = static_cast<int32_t>(_labels.labelsArray.size());
            if (nValues == 0) {
                LERROR("Error writing cache: No values were loaded");
                return false;
            }
            fileStream.write(reinterpret_cast<const char*>(&nValues), sizeof(int32_t));

            size_t nBytes = nValues * sizeof(_labels.labelsArray[0]);
            fileStream.write(reinterpret_cast<const char*>(&_labels.labelsArray[0]), nBytes);

            bool success = fileStream.good();
            return success;
        }
        else {
            LERROR(fmt::format("Error opening file '{}' for save cache file", file));
            return false;
        }
    }

    void GlobeLabelsComponent::draw(const RenderData& data) {
        // Calculate the MVP matrix
        glm::dmat4 viewTransform = glm::dmat4(data.camera.combinedViewMatrix());
        glm::dmat4 vp = glm::dmat4(data.camera.sgctInternal.projectionMatrix()) *
            viewTransform;
        glm::dmat4 mvp = vp * _globe->modelTransform();
        // Render labels
        if (_labelsEnabled) {
            glm::dmat4 invMVP = glm::inverse(mvp);
            glm::dvec3 orthoRight = glm::dvec3(
                glm::normalize(glm::dvec3(invMVP * glm::dvec4(1.0, 0.0, 0.0, 0.0)))
            );
            glm::dvec3 orthoUp = glm::dvec3(
                glm::normalize(glm::dvec3(invMVP * glm::dvec4(0.0, 1.0, 0.0, 0.0)))
            );

            double distToCamera = glm::length(data.camera.positionVec3() -
                glm::dvec3(_globe->modelTransform() * glm::vec4(0.0, 0.0, 0.0, 1.0)));

            float fadeInVariable = 1.f;
            if (_labelsFadeInEnabled) {
                glm::dvec2 fadeRange = glm::dvec2(
                    _globe->ellipsoid().averageRadius() + _labelsMinHeight
                );
                fadeRange.x += _labelsFadeInDist;
                double a = 1.0 / (fadeRange.y - fadeRange.x);
                double b = -(fadeRange.x / (fadeRange.y - fadeRange.x));
                double funcValue = a * distToCamera + b;
                fadeInVariable *= funcValue > 1.0 ? 1.f : static_cast<float>(funcValue);

                if (fadeInVariable < 0.005f) {
                    return;
                }
            }

            renderLabels(data, mvp, orthoRight, orthoUp, distToCamera, fadeInVariable);
        }
    }

    void GlobeLabelsComponent::renderLabels(const RenderData& data,
        const glm::dmat4& modelViewProjectionMatrix, const glm::dvec3& orthoRight,
        const glm::dvec3& orthoUp, const float distToCamera, const float fadeInVariable) {

        glm::vec4 textColor = _labelsColor;
        textColor.a *= fadeInVariable;
        const float DIST_EPS = 2500.f;

        glm::dvec3 oP = glm::dvec3(_globe->modelTransform() * glm::vec4(0.0, 0.0, 0.0, 1.0));
        for (const LabelEntry lEntry : _labels.labelsArray) {
            glm::vec3 position = lEntry.geoPosition;
            float distCameraToPoint = glm::length(data.camera.positionVec3() -
                glm::dvec3(_globe->modelTransform() * glm::vec4(position, 1.0)));
            if (distToCamera >= (distCameraToPoint + DIST_EPS)) { // culling
                position += _labelsMinHeight;
                ghoul::fontrendering::FontRenderer::defaultProjectionRenderer().render(
                    *_font,
                    position,
                    textColor,
                    powf(2.f, _labelsSize),
                    _labelsMinSize,
                    1000,
                    modelViewProjectionMatrix,
                    orthoRight,
                    orthoUp,
                    data.camera.positionVec3(),
                    data.camera.lookUpVectorWorldSpace(),
                    0,
                    "%s",
                    lEntry.feature
                );
            }
        }
    }

    } // namespace openspace
