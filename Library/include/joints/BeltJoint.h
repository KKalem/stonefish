//
//  BeltJoint.h
//  Stonefish
//
//  Created by Patryk Cieslak on 28/03/2014.
//  Copyright (c) 2014 Patryk Cieslak. All rights reserved.
//

#ifndef __Stonefish_BeltJoint__
#define __Stonefish_BeltJoint__

#include "joints/Joint.h"

namespace sf
{

class BeltJoint : public Joint
{
public:
    BeltJoint(std::string uniqueName, SolidEntity* solidA, SolidEntity* solidB, const Vector3& axisA, const Vector3& axisB, Scalar ratio);
    
    Vector3 Render();
    
    JointType getType();
    Scalar getRatio();
    
    //Not applicable
    void ApplyDamping(){}
    bool SolvePositionIC(Scalar linearTolerance, Scalar angularTolerance){ return true; }
    
private:
    Scalar gearRatio;
};
    
}

#endif