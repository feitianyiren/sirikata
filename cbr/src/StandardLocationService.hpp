/*  Sirikata
 *  StandardLocationService.hpp
 *
 *  Copyright (c) 2009, Ewen Cheslack-Postava
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of Sirikata nor the names of its contributors may
 *    be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SIRIKATA_STANDARD_LOCATION_SERVICE_HPP_
#define _SIRIKATA_STANDARD_LOCATION_SERVICE_HPP_

#include "LocationService.hpp"

namespace Sirikata {

/** Standard location service, which functions entirely based on location
 *  updates from objects and other spaces servers.
 */
class StandardLocationService : public LocationService {
public:
    StandardLocationService(SpaceContext* ctx);
    // FIXME add constructor which can add all the objects being simulated to mLocations

    virtual bool contains(const UUID& uuid) const;
    virtual TrackingType type(const UUID& uuid) const;

    virtual void service();

    virtual TimedMotionVector3f location(const UUID& uuid);
    virtual Vector3f currentPosition(const UUID& uuid);
    virtual BoundingSphere3f bounds(const UUID& uuid);

    virtual void addLocalObject(const UUID& uuid, const TimedMotionVector3f& loc, const BoundingSphere3f& bounds);
    virtual void removeLocalObject(const UUID& uuid);

    virtual void addReplicaObject(const Time& t, const UUID& uuid, const TimedMotionVector3f& loc, const BoundingSphere3f& bounds);
    virtual void removeReplicaObject(const Time& t, const UUID& uuid);

    virtual void receiveMessage(Message* msg);
    virtual void receiveMessage(const Sirikata::Protocol::Object::ObjectMessage& msg);

    virtual void locationUpdate(UUID source, void* buffer, uint32 length);

private:
    struct LocationInfo {
        TimedMotionVector3f location;
        BoundingSphere3f bounds;
        bool local;
    };
    typedef std::tr1::unordered_map<UUID, LocationInfo, UUID::Hasher> LocationMap;

    LocationMap mLocations;
}; // class StandardLocationService

} // namespace Sirikata

#endif //_SIRIKATA_STANDARD_LOCATION_SERVICE_HPP_
