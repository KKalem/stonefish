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
//  Entity.h
//  Stonefish
//
//  Created by Patryk Cieslak on 11/28/12.
//  Copyright (c) 2012-2019 Patryk Cieslak. All rights reserved.
//

#ifndef __Stonefish_Entity__
#define __Stonefish_Entity__

#define BIT(x) (1<<(x))

#include "StonefishCommon.h"

namespace sf
{
    //! An enum specifying the type of entity.
    typedef enum { ENTITY_STATIC, ENTITY_SOLID, ENTITY_FEATHERSTONE, ENTITY_FORCEFIELD } EntityType;
    
    //! An enum used for collision filtering.
    typedef enum
    {
        MASK_NONCOLLIDING = 0,
        MASK_STATIC = BIT(0),
        MASK_DEFAULT = BIT(1)
    }
    CollisionMask;
    
    //! An enum defining how the body is displayed.
    typedef enum {DISPLAY_GRAPHICAL = 0, DISPLAY_PHYSICAL} DisplayMode;
    
    struct Renderable;
    class SimulationManager;
    
    //! An abstract class representing a simulation entity.
    class Entity
    {
    public:
        //! A constructor.
        /*!
         \param uniqueName a name for the entity
         */
        Entity(std::string uniqueName);
        
        //! A destructor.
        virtual ~Entity();
        
        //! A method used to set if the entity should be renderable.
        /*!
         \param render a flag informing if the entity should be rendered
         */
        void setRenderable(bool render);
        
        //! A method informing if the entity is renderable.
        bool isRenderable() const;
        
        //! A method returning the name of the entity.
        std::string getName() const;
        
        //! A method returning the type of the entity.
        virtual EntityType getType() = 0;
        
        //! A method implementing rendering of the entity.
        virtual std::vector<Renderable> Render() = 0;
        
        //! A method used to add the entity to the simulation.
        /*!
         \param sm a pointer to a simulation manager
         */
        virtual void AddToSimulation(SimulationManager* sm) = 0;
        
        //! A method returning the extents of the entity axis alligned bounding box.
        /*!
         \param min a point located at the minimum coordinate corner
         \param max a point located at the maximum coordinate corner
         */
        virtual void getAABB(Vector3& min, Vector3& max) = 0;
        
    private:
        bool renderable;
        std::string name;
    };
}

#endif
