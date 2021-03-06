/*    
    This file is a part of Stonefish.

    Stonefish is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Stonefish is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

//
//  VelocityField.h
//  Stonefish
//
//  Created by Patryk Cieslak on 10/07/18.
//  Copyright (c) 2018-2019 Patryk Cieslak. All rights reserved.
//

#ifndef __Stonefish_VelocityField__
#define __Stonefish_VelocityField__

#include "StonefishCommon.h"
#include "graphics/OpenGLContent.h"

namespace sf
{
    //! An abstract class representing a velocity field.
    class VelocityField
    {
    public:
        //! A constructor.
        VelocityField();
        
        //! A destructor.
        virtual ~VelocityField();
        
        //! A method returning velocity at a specified point.
        /*!
         \param p a point at which the velocity is requested
         \return velocity [m/s]
         */
        virtual Vector3 GetVelocityAtPoint(const Vector3& p) = 0;
        
        //! A method implementing the rendering of the velocity field.
        virtual std::vector<Renderable> Render() = 0;
    };
}

#endif
