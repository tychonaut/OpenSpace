
#include <openspace/util/distancetoobject.h>
#include <cassert>
//#include <openspace/util/camera.h>
#include <openspace/util/powerscaledcoordinate.h>
//#include <assert.h>

#include <openspace/engine/openspaceengine.h>
#include <openspace/interaction/interactionhandler.h>
//#include <openspace/util/constants.h>
#include <openspace/util/spicemanager.h>
#include <openspace/util/syncbuffer.h>

#include <ghoul/filesystem/filesystem.h>

namespace {
    const std::string _loggerCat = "Distance to object";
}


namespace openspace {

    DistanceToObject* DistanceToObject::_distInstance = nullptr;
    DistanceToObject::DistanceToObject() {}
    int sceneNr = 1;

    bool DistanceToObject::initialize() {
        assert(_distInstance == nullptr);
        _distInstance = new DistanceToObject();
        assert(sceneNr == 1);
        return true;
    }

    void DistanceToObject::deinitialize() {
        assert(_distInstance);
        delete _distInstance;
        _distInstance = nullptr;
    }



    DistanceToObject& DistanceToObject::ref() {
        assert(_distInstance);
        return *_distInstance;
    }


    /*
    static bool isInitialized(){

    return (_instance != nullptr);
    }*/


    /*
    float DistanceToObject::distanceCalc(const psc& positionPSC, psc targetPosPSC) //psc& camerapos
    {
        glm::dvec3 targetPos = targetPosPSC.dvec3();
        glm::dvec3 position = positionPSC.dvec3();

        distance = sqrt(pow(position[0] - targetPos[0], 2) + pow(position[1] - targetPos[1], 2) + pow(position[2] - targetPos[2], 2));
        //if (distance != nullptr)
        
        return distance;

    }
    */

    double DistanceToObject::distanceCalc(const psc& positionPSC, psc targetPosPSC)
    {
        // JCC: Changed the distance calculations from meters to killometers.
        glm::dvec3 targetPos = targetPosPSC.dvec3()/1000.0;
        glm::dvec3 position = positionPSC.dvec3()/1000.0;

        return sqrt(pow(position[0] - targetPos[0], 2) + pow(position[1] - targetPos[1], 2) + pow(position[2] - targetPos[2], 2));		
    }



}