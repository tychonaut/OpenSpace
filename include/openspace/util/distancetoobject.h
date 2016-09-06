
#ifndef __DISTANCETOOBJ_H__
#define __DISTANCETOOBJ_H__

#include <openspace/util/powerscaledcoordinate.h>


#include <glm/gtc/type_ptr.hpp>

#include <iostream>
namespace openspace {

class DistanceToObject {
public:
    static bool initialize();
    static void deinitialize();
    static int sceneNrPub;

    /*
    * Returns the reference to the distance singleton object.
    * \return The reference to the distance singleton object
    */

    static DistanceToObject& ref();

    // static bool isInitialized(); Not working?


    //float distanceCalc(); //psc& camerapos
    //const psc& camera
    //const distancefromcamerato(int a);

    //float distanceCalc(const psc& position, psc targetPos); //psc& camerapos //const ?
    double distanceCalc(const psc& position, psc targetPos);
private:
    /// Creates the distance object. Only used in the initialize() method
    DistanceToObject();
    double distToScene = 500000.0;
    DistanceToObject(const DistanceToObject& src) = delete;

    DistanceToObject& operator=(const DistanceToObject& rhs) = delete;
    double distance;
    static DistanceToObject* _distInstance; ///< The singleton instance
                                            //float distance;

                                            //float _distancefromcamera;
                                            //static DistanceToObject* _distancefromcamera; ///< The singleton instance

};

    /**
    * Returns the reference to the Time singleton object.
    * \return The reference to the Time singleton object
    */

}
#endif // __DISTANCETOOBJ_H__