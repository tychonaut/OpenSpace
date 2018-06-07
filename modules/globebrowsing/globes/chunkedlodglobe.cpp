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

#include <modules/globebrowsing/globes/chunkedlodglobe.h>

#include <modules/globebrowsing/globebrowsingmodule.h>
#include <modules/globebrowsing/chunk/chunk.h>
#include <modules/globebrowsing/chunk/chunklevelevaluator/chunklevelevaluator.h>
#include <modules/globebrowsing/chunk/chunklevelevaluator/availabletiledataevaluator.h>
#include <modules/globebrowsing/chunk/chunklevelevaluator/distanceevaluator.h>
#include <modules/globebrowsing/chunk/chunklevelevaluator/projectedareaevaluator.h>
#include <modules/globebrowsing/chunk/chunknode.h>
#include <modules/globebrowsing/chunk/culling/chunkculler.h>
#include <modules/globebrowsing/chunk/culling/frustumculler.h>
#include <modules/globebrowsing/chunk/culling/horizonculler.h>
#include <modules/globebrowsing/globes/renderableglobe.h>
#include <modules/globebrowsing/meshes/skirtedgrid.h>
#include <modules/globebrowsing/tile/tileprovider/tileprovider.h>
#include <modules/globebrowsing/rendering/chunkrenderer.h>
#include <modules/globebrowsing/rendering/layer/layergroup.h>
#include <modules/globebrowsing/rendering/layer/layermanager.h>
#include <modules/debugging/rendering/debugrenderer.h>
#include <modules/globebrowsing/tile/tileindex.h>

#include <openspace/util/time.h>
#include <openspace/engine/openspaceengine.h>
#include <openspace/engine/moduleengine.h>
#include <openspace/engine/wrapper/windowwrapper.h>

#include <ghoul/filesystem/filesystem.h>
#include <ghoul/opengl/texture.h>
#include <ghoul/font/fontmanager.h>
#include <ghoul/font/fontrenderer.h>

#include <math.h>

namespace openspace::globebrowsing {

const TileIndex ChunkedLodGlobe::LEFT_HEMISPHERE_INDEX = TileIndex(0, 0, 1);
const TileIndex ChunkedLodGlobe::RIGHT_HEMISPHERE_INDEX = TileIndex(1, 0, 1);
const GeodeticPatch ChunkedLodGlobe::COVERAGE = GeodeticPatch(0, 0, 90, 180);

ChunkedLodGlobe::ChunkedLodGlobe(const RenderableGlobe& owner, size_t segmentsPerPatch,
    std::shared_ptr<LayerManager> layerManager,
    Ellipsoid& ellipsoid)
    : Renderable({ { "Identifier", owner.identifier() }, { "Name", owner.guiName() } })
    , minSplitDepth(2)
    , maxSplitDepth(22)
    , stats(StatsCollector(absPath("test_stats"), 1, StatsCollector::Enabled::No))
    , _owner(owner)
    , _leftRoot(std::make_unique<ChunkNode>(Chunk(owner, LEFT_HEMISPHERE_INDEX)))
    , _rightRoot(std::make_unique<ChunkNode>(Chunk(owner, RIGHT_HEMISPHERE_INDEX)))
    , _layerManager(layerManager)
    , _shadersNeedRecompilation(true)
    , _labelsEnabled(false)
    , _fontSize(30)
    , _labelsMinSize(4)
    , _labelsSize(2.5f)
    , _labelsMinHeight(100.f)
    , _labelsColor(1.f)
    , _labelsFadeInDistance(1000000.f)
    , _labelsFadeInEnabled(true)
    , _labelsCullingDisabled(false)
{
    auto geometry = std::make_shared<SkirtedGrid>(
        static_cast<unsigned int>(segmentsPerPatch),
        static_cast<unsigned int>(segmentsPerPatch),
        TriangleSoup::Positions::No,
        TriangleSoup::TextureCoordinates::Yes,
        TriangleSoup::Normals::No
    );

    _chunkCullers.push_back(std::make_unique<culling::HorizonCuller>());
    _chunkCullers.push_back(std::make_unique<culling::FrustumCuller>(
        AABB3(glm::vec3(-1, -1, 0), glm::vec3(1, 1, 1e35)))
    );

    _chunkEvaluatorByAvailableTiles =
        std::make_unique<chunklevelevaluator::AvailableTileData>();
    _chunkEvaluatorByProjectedArea =
    std::make_unique<chunklevelevaluator::ProjectedArea>();
    _chunkEvaluatorByDistance =
    std::make_unique<chunklevelevaluator::Distance>();

    _renderer = std::make_unique<ChunkRenderer>(
        geometry,
        layerManager,
        ellipsoid
    );
}

// The destructor is defined here to make it feasiable to use a unique_ptr
// with a forward declaration
ChunkedLodGlobe::~ChunkedLodGlobe() {}

bool ChunkedLodGlobe::isReady() const {
    return true;
}

std::shared_ptr<LayerManager> ChunkedLodGlobe::layerManager() const {
    return _layerManager;
}

bool ChunkedLodGlobe::testIfCullable(const Chunk& chunk,
                                     const RenderData& renderData) const
{
    if (_owner.debugProperties().performHorizonCulling &&
        _chunkCullers[0]->isCullable(chunk, renderData)) {
        return true;
    }
    if (_owner.debugProperties().performFrustumCulling &&
        _chunkCullers[1]->isCullable(chunk, renderData)) {
        return true;
    }
    return false;
}

const ChunkNode& ChunkedLodGlobe::findChunkNode(const Geodetic2& p) const {
    ghoul_assert(COVERAGE.contains(p),
        "Point must be in lat [-90, 90] and lon [-180, 180]");

    return p.lon < COVERAGE.center().lon ? _leftRoot->find(p) : _rightRoot->find(p);
}

int ChunkedLodGlobe::getDesiredLevel(
    const Chunk& chunk, const RenderData& renderData) const {
    int desiredLevel = 0;
    if (_owner.debugProperties().levelByProjectedAreaElseDistance) {
        desiredLevel = _chunkEvaluatorByProjectedArea->getDesiredLevel(chunk, renderData);
    }
    else {
        desiredLevel = _chunkEvaluatorByDistance->getDesiredLevel(chunk, renderData);
    }

    int levelByAvailableData = _chunkEvaluatorByAvailableTiles->getDesiredLevel(
        chunk, renderData
    );
    if (levelByAvailableData != chunklevelevaluator::Evaluator::UnknownDesiredLevel &&
        _owner.debugProperties().limitLevelByAvailableData)
    {
        desiredLevel = glm::min(desiredLevel, levelByAvailableData);
    }

    desiredLevel = glm::clamp(desiredLevel, minSplitDepth, maxSplitDepth);
    return desiredLevel;
}

float ChunkedLodGlobe::getHeight(glm::dvec3 position) const {
    float height = 0;

    // Get the uv coordinates to sample from
    Geodetic2 geodeticPosition = _owner.ellipsoid().cartesianToGeodetic2(position);
    int chunkLevel = findChunkNode(geodeticPosition).getChunk().tileIndex().level;

    TileIndex tileIndex = TileIndex(geodeticPosition, chunkLevel);
    GeodeticPatch patch = GeodeticPatch(tileIndex);

    Geodetic2 geoDiffPatch =
        patch.getCorner(Quad::NORTH_EAST) - patch.getCorner(Quad::SOUTH_WEST);

    Geodetic2 geoDiffPoint = geodeticPosition - patch.getCorner(Quad::SOUTH_WEST);
    glm::vec2 patchUV = glm::vec2(
        geoDiffPoint.lon / geoDiffPatch.lon,
        geoDiffPoint.lat / geoDiffPatch.lat
    );

    // Get the tile providers for the height maps
    const std::vector<std::shared_ptr<Layer>>& heightMapLayers =
        _layerManager->layerGroup(layergroupid::GroupID::HeightLayers).activeLayers();

    for (const std::shared_ptr<Layer>& layer : heightMapLayers) {
        tileprovider::TileProvider* tileProvider = layer->tileProvider();
        if (!tileProvider) {
            continue;
        }
        // Transform the uv coordinates to the current tile texture
        ChunkTile chunkTile = tileProvider->getChunkTile(tileIndex);
        const Tile& tile = chunkTile.tile;
        const TileUvTransform& uvTransform = chunkTile.uvTransform;
        const TileDepthTransform& depthTransform = tileProvider->depthTransform();
        if (tile.status() != Tile::Status::OK) {
            return 0;
        }

        ghoul::opengl::Texture* tileTexture = tile.texture();
        if (!tileTexture) {
            return 0;
        }

        glm::vec2 transformedUv = layer->TileUvToTextureSamplePosition(
            uvTransform,
            patchUV,
            glm::uvec2(tileTexture->dimensions())
        );

        // Sample and do linear interpolation
        // (could possibly be moved as a function in ghoul texture)
        // Suggestion: a function in ghoul::opengl::Texture that takes uv coordinates
        // in range [0,1] and uses the set interpolation method and clamping.

        glm::uvec3 dimensions = tileTexture->dimensions();

        glm::vec2 samplePos = transformedUv * glm::vec2(dimensions);
        glm::uvec2 samplePos00 = samplePos;
        samplePos00 = glm::clamp(
            samplePos00,
            glm::uvec2(0, 0),
            glm::uvec2(dimensions) - glm::uvec2(1)
        );
        glm::vec2 samplePosFract = samplePos - glm::vec2(samplePos00);

        glm::uvec2 samplePos10 = glm::min(
            samplePos00 + glm::uvec2(1, 0),
            glm::uvec2(dimensions) - glm::uvec2(1)
        );
        glm::uvec2 samplePos01 = glm::min(
            samplePos00 + glm::uvec2(0, 1),
            glm::uvec2(dimensions) - glm::uvec2(1)
        );
        glm::uvec2 samplePos11 = glm::min(
            samplePos00 + glm::uvec2(1, 1),
            glm::uvec2(dimensions) - glm::uvec2(1)
        );

        float sample00 = tileTexture->texelAsFloat(samplePos00).x;
        float sample10 = tileTexture->texelAsFloat(samplePos10).x;
        float sample01 = tileTexture->texelAsFloat(samplePos01).x;
        float sample11 = tileTexture->texelAsFloat(samplePos11).x;

        // In case the texture has NaN or no data values don't use this height map.
        bool anySampleIsNaN =
            std::isnan(sample00) ||
            std::isnan(sample01) ||
            std::isnan(sample10) ||
            std::isnan(sample11);

        bool anySampleIsNoData =
            sample00 == tileProvider->noDataValueAsFloat() ||
            sample01 == tileProvider->noDataValueAsFloat() ||
            sample10 == tileProvider->noDataValueAsFloat() ||
            sample11 == tileProvider->noDataValueAsFloat();

        if (anySampleIsNaN || anySampleIsNoData) {
            continue;
        }

        float sample0 = sample00 * (1.f - samplePosFract.x) + sample10 * samplePosFract.x;
        float sample1 = sample01 * (1.f - samplePosFract.x) + sample11 * samplePosFract.x;

        float sample = sample0 * (1.f - samplePosFract.y) + sample1 * samplePosFract.y;

        // Same as is used in the shader. This is not a perfect solution but
        // if the sample is actually a no-data-value (min_float) the interpolated
        // value might not be. Therefore we have a cut-off. Assuming no data value
        // is smaller than -100000
        if (sample > -100000) {
            // Perform depth transform to get the value in meters
            height = depthTransform.depthOffset + depthTransform.depthScale * sample;
            // Make sure that the height value follows the layer settings.
            // For example if the multiplier is set to a value bigger than one,
            // the sampled height should be modified as well.
            height = layer->renderSettings().performLayerSettings(height);
        }
    }
    // Return the result
    return height;
}

void ChunkedLodGlobe::notifyShaderRecompilation() {
    _shadersNeedRecompilation = true;
}

void ChunkedLodGlobe::recompileShaders() {
    _renderer->recompileShaders(_owner);
    _shadersNeedRecompilation = false;
}

void ChunkedLodGlobe::initializeFonts() {
    if (_font == nullptr) {
        _font = OsEng.fontManager().font(
            "Mono",
            static_cast<float>(_fontSize),
            ghoul::fontrendering::FontManager::Outline::Yes,
            ghoul::fontrendering::FontManager::LoadGlyphs::No
        );
    }
}

void ChunkedLodGlobe::setLabels(RenderableGlobe::Labels & labels) {
    _labels = std::move(labels);
}

void ChunkedLodGlobe::setFontSize(const int size) {
    _fontSize = std::move(size);
    if (_font) {
        _font = OsEng.fontManager().font(
            "Mono",
            static_cast<float>(_fontSize),
            ghoul::fontrendering::FontManager::Outline::Yes,
            ghoul::fontrendering::FontManager::LoadGlyphs::No
        );
    }
}

void ChunkedLodGlobe::enableLabelsRendering(const bool enable) {
    _labelsEnabled = std::move(enable);
}

void ChunkedLodGlobe::setLabelsSize(const float size) {
    _labelsSize = std::move(size);
}

void ChunkedLodGlobe::setLabelsMinHeight(const float height) {
    _labelsMinHeight = std::move(height);
}

void ChunkedLodGlobe::setLabelsColor(const glm::vec4 & color) {
    _labelsColor = std::move(color);
}

void ChunkedLodGlobe::setLabelFadeInDistance(const float dist) {
    _labelsFadeInDistance = std::move(dist);
}

void ChunkedLodGlobe::setLabelsMinSize(const int size) {
    _labelsMinSize = std::move(size);
}

void ChunkedLodGlobe::enableLabelsFadeIn(const bool enabled) {
    _labelsFadeInEnabled = std::move(enabled);
}

void ChunkedLodGlobe::disableLabelsCulling(const bool disabled) {
    _labelsCullingDisabled = std::move(disabled);
}

void ChunkedLodGlobe::render(const RenderData& data, RendererTasks&) {
    
    // Calculate the MVP matrix
    glm::dmat4 viewTransform = glm::dmat4(data.camera.combinedViewMatrix());
    glm::dmat4 vp = glm::dmat4(data.camera.sgctInternal.projectionMatrix()) *
        viewTransform;
    glm::dmat4 mvp = vp * _owner.modelTransform();

    stats.startNewRecord();
    if (_shadersNeedRecompilation) {
        _renderer->recompileShaders(_owner);
        _shadersNeedRecompilation = false;
    }

    auto duration = std::chrono::system_clock::now().time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    stats.i["time"] = millis;

    _leftRoot->updateChunkTree(data);
    _rightRoot->updateChunkTree(data);

    // Render function
    auto renderJob = [this, &data, &mvp](const ChunkNode& chunkNode) {
        stats.i["chunks nodes"]++;
        const Chunk& chunk = chunkNode.getChunk();
        if (chunkNode.isLeaf()) {
            stats.i["leafs chunk nodes"]++;
            if (chunk.isVisible()) {
                stats.i["rendered chunks"]++;
                _renderer->renderChunk(chunkNode.getChunk(), data);
                debugRenderChunk(chunk, mvp);
            }
        }
    };

    _leftRoot->breadthFirst(renderJob);
    _rightRoot->breadthFirst(renderJob);

    //_leftRoot->reverseBreadthFirst(renderJob);
    //_rightRoot->reverseBreadthFirst(renderJob);

    auto duration2 = std::chrono::system_clock::now().time_since_epoch();
    auto ms2 = std::chrono::duration_cast<std::chrono::milliseconds>(duration2).count();
    stats.i["chunk globe render time"] = ms2 - millis;

    // Render labels
    if (_labelsEnabled) {
        glm::dmat4 invModelMatrix = glm::inverse(_owner.modelTransform());
        
        glm::dvec3 cameraViewDirectionObj = glm::dvec3(
            invModelMatrix * glm::dvec4(data.camera.viewDirectionWorldSpace(), 0.0)
            );
        glm::dvec3 cameraUpDirectionObj = glm::dvec3(
            invModelMatrix * glm::dvec4(data.camera.lookUpVectorWorldSpace(), 0.0)
            );
        glm::dvec3 orthoRight = glm::normalize(
            glm::cross(cameraViewDirectionObj, cameraUpDirectionObj)
        );
        if (orthoRight == glm::dvec3(0.0)) {
            glm::dvec3 otherVector(
                cameraUpDirectionObj.y,
                cameraUpDirectionObj.x,
                cameraUpDirectionObj.z
            );
            orthoRight = glm::normalize(glm::cross(otherVector, cameraViewDirectionObj));
        }
        glm::dvec3 orthoUp = glm::normalize(
            glm::cross(orthoRight, cameraViewDirectionObj)
        );



        double distToCamera = glm::length(data.camera.positionVec3() -
            glm::dvec3(_owner.modelTransform() * glm::vec4(0.0, 0.0, 0.0, 1.0)));

        float fadeInVariable = 1.f;
        if (_labelsFadeInEnabled) {
            glm::dvec2 fadeRange = glm::dvec2(
                _owner.ellipsoid().averageRadius() + _labelsMinHeight
            );
            fadeRange.x += _labelsFadeInDistance;
            double a = 1.0 / (fadeRange.y - fadeRange.x);
            double b = -(fadeRange.x / (fadeRange.y - fadeRange.x));
            double funcValue = a * distToCamera + b;
            fadeInVariable *= funcValue > 1.0 ? 1.f : static_cast<float>(funcValue);

            if (fadeInVariable < 0.005f) {
                return;
            }
        }

        //distToCamera = length(glm::dvec3(invModelMatrix * glm::dvec4(data.camera.positionVec3(), 1.0)));
        renderLabels(data, mvp, orthoRight, orthoUp, distToCamera, fadeInVariable);
    }
}

void ChunkedLodGlobe::renderLabels(const RenderData& data,
    const glm::dmat4& modelViewProjectionMatrix, const glm::dvec3& orthoRight,
    const glm::dvec3& orthoUp, const float distToCamera, const float fadeInVariable) {
    
    glm::vec4 textColor = _labelsColor;
    textColor.a *= fadeInVariable;
    const double DIST_EPS = 5500.0;
    const double SIN_EPS = 0.04;
    
    int textRenderingTechnique = 0;
    if (OsEng.windowWrapper().isFisheyeRendering()) {
        textRenderingTechnique = 1;
    }
    
    glm::dmat4 invMP = glm::inverse(_owner.modelTransform());
    glm::dvec3 cameraPosObj = glm::dvec3(invMP * glm::dvec4(data.camera.positionVec3(), 1.0));
    glm::dvec3 cameraLookUpObj = glm::dvec3(invMP * glm::dvec4(data.camera.lookUpVectorWorldSpace(), 0.0));

    glm::dvec3 globePositionWorld = glm::dvec3(_owner.modelTransform() * glm::vec4(0.0, 0.0, 0.0, 1.0));
    glm::dvec3 cameraToGlobeDistanceWorld = globePositionWorld - data.camera.positionVec3();
    double distanceCameraGlobeWorld = glm::length(cameraToGlobeDistanceWorld);

    double distanceGlobeToCameraObj = 
        glm::length(glm::dvec3(invMP * glm::dvec4(data.camera.positionVec3(), 1.0)));

    for (const RenderableGlobe::LabelEntry lEntry: _labels.labelsArray) {
        glm::vec3 position = lEntry.geoPosition;
        glm::dvec3 locationPositionWorld = 
            glm::dvec3(_owner.modelTransform() * glm::dvec4(position, 1.0));
        double distanceCameraToLocationWorld = 
            glm::length(locationPositionWorld - data.camera.positionVec3());
        double distanceLocationObj = glm::length(glm::dvec3(position));
        double sinAlpha = distanceLocationObj / distanceGlobeToCameraObj;
        double maxSinAlpha = _owner.ellipsoid().maximumRadius() / distanceGlobeToCameraObj;
        
        bool draw = false;
        if (_labelsCullingDisabled) {
            draw = true;
        }
        else if ((distanceCameraGlobeWorld >= (distanceCameraToLocationWorld + DIST_EPS)) &&
            (sinAlpha <= (maxSinAlpha + SIN_EPS))) { // culling
            draw = true;
        }

        if (draw) {
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
                cameraPosObj,
                cameraLookUpObj,
                textRenderingTechnique,
                "%s",
                lEntry.feature
            );
        }
    }
 }

void ChunkedLodGlobe::debugRenderChunk(const Chunk& chunk, const glm::dmat4& mvp) const {
    if (_owner.debugProperties().showChunkBounds ||
        _owner.debugProperties().showChunkAABB)
    {
        std::vector<glm::dvec4> modelSpaceCorners = chunk.boundingPolyhedronCorners();
        std::vector<glm::vec4> clippingSpaceCorners(8);
        AABB3 screenSpaceBounds;

        for (size_t i = 0; i < 8; ++i) {
            const glm::vec4& clippingSpaceCorner = mvp * modelSpaceCorners[i];
            clippingSpaceCorners[i] = clippingSpaceCorner;

            glm::vec3 screenSpaceCorner =
                glm::vec3((1.0f / clippingSpaceCorner.w) * clippingSpaceCorner);
            screenSpaceBounds.expand(screenSpaceCorner);
        }

        unsigned int colorBits = 1 + chunk.tileIndex().level % 6;
        glm::vec4 color = glm::vec4(colorBits & 1, colorBits & 2, colorBits & 4, 0.3);

        if (_owner.debugProperties().showChunkBounds) {
            DebugRenderer::ref().renderNiceBox(clippingSpaceCorners, color);
        }

        if (_owner.debugProperties().showChunkAABB) {
            auto& screenSpacePoints =
                DebugRenderer::ref().verticesFor(screenSpaceBounds);
            DebugRenderer::ref().renderNiceBox(screenSpacePoints, color);
        }
    }
}

void ChunkedLodGlobe::update(const UpdateData& data) {
    setBoundingSphere(static_cast<float>(
        _owner.ellipsoid().maximumRadius() * data.modelTransform.scale
    ));
    _renderer->update();
}

} // namespace openspace::globebrowsing
