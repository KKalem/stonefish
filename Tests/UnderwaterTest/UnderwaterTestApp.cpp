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
//  UnderwaterTestApp.cpp
//  Stonefish
//
//  Created by Patryk Cieslak on 03/03/2014.
//  Copyright (c) 2014-2019 Patryk Cieslak. All rights reserved.
//

#include "UnderwaterTestApp.h"

#include <actuators/Thruster.h>
#include <actuators/VariableBuoyancy.h>
#include <core/Robot.h>
#include <graphics/IMGUI.h>
#include <utils/SystemUtil.hpp>
#include <comms/USBL.h>
#include <core/Console.h>

UnderwaterTestApp::UnderwaterTestApp(std::string dataDirPath, sf::RenderSettings s, sf::HelperSettings h, UnderwaterTestManager* sim)
    : GraphicalSimulationApp("Underwater Test", dataDirPath, s, h, sim)
{
}

void UnderwaterTestApp::InitializeGUI()
{
    largePrint = new sf::OpenGLPrinter(sf::GetShaderPath() + std::string(STANDARD_FONT_NAME), 64.0);
}

void UnderwaterTestApp::DoHUD()
{
    GraphicalSimulationApp::DoHUD();
    
    /*
    sf::USBL* usbl = (sf::USBL*)getSimulationManager()->getComm("GIRONA500/USBL");
    std::map<uint64_t, std::pair<sf::Scalar, sf::Vector3>>& transPos = usbl->getTransponderPositions();
    std::map<uint64_t, std::pair<sf::Scalar, sf::Vector3>>::iterator it = transPos.begin();
    sf::Vector3 pos = it->second.second;
    uint64_t id = it->first;
    
    printf("Transponder %ld: %1.3lf, %1.3lf, %1.3lf\n", id, pos.getX(), pos.getY(), pos.getZ());
    */
    
    /*
    sf::Vector3 pos;
    std::string frame;
    ((sf::AcousticModem*)getSimulationManager()->getComm("Modem"))->getPosition(pos, frame); 
    cInfo("Modem pos: %1.3lf, %1.3lf, %1.3lf frame: %s", pos.getX(), pos.getY(), pos.getZ(), frame.c_str());
    */
    
    /*
    sf::Uid id;
    id.owner = 5;
    id.item = 0;
    
    std::vector<std::string> robotNames;
    robotNames.push_back("Girona500");
    robotNames.push_back("Girona1000");
    robotNames.push_back("Girona2000");
    option = getGUI()->DoComboBox(id, 180.f, 200.f, 150.f, robotNames, option, "Trackball center");
    */
    
    //sf::VariableBuoyancy* vbs = (sf::VariableBuoyancy*)getSimulationManager()->getRobot("GIRONA500")->getActuator("VBS");
    //vbs->setFlowRate((getGUI()->DoSlider(id, 180.f, 10.f, 150.f, sf::Scalar(-0.0005), sf::Scalar(0.0005), vbs->getFlowRate(), "VBS flow rate", 4)));
    
    /*
    sf::Thruster* th = (sf::Thruster*)getSimulationManager()->getRobot("GIRONA500")->getActuator("ThrusterHeaveStern");
    sf::Thruster* th2 = (sf::Thruster*)getSimulationManager()->getRobot("GIRONA500")->getActuator("ThrusterHeaveBow");
    sf::Scalar sp = (getGUI()->DoSlider(id, 180.f, 10.f, 150.f, sf::Scalar(-1), sf::Scalar(1), th->getSetpoint(), "ThrusterHeaveStern"));
    th->setSetpoint(sp);
    th2->setSetpoint(sp);
    */
}
