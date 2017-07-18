//
//  Cylinder.h
//  Stonefish
//
//  Created by Patryk Cieslak on 1/30/13.
//  Copyright (c) 2013 Patryk Cieslak. All rights reserved.
//

#ifndef __Stonefish_Cylinder__
#define __Stonefish_Cylinder__

#include "SolidEntity.h"

class Cylinder : public SolidEntity
{
public:
    Cylinder(std::string uniqueName, btScalar cylinderRadius, btScalar cylinderHeight, Material m, int lookId = -1);
    ~Cylinder();
    
    SolidEntityType getSolidType();
    btCollisionShape* BuildCollisionShape();

private:
    btScalar radius;
    btScalar halfHeight;
};

#endif