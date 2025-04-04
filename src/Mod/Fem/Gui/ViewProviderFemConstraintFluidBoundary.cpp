/***************************************************************************
 *   Copyright (c) 2013 Jan Rheinländer                                    *
 *                                   <jrheinlaender@users.sourceforge.net> *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/

#include "PreCompiled.h"

#ifndef _PreComp_
#include <Inventor/SbRotation.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/nodes/SoMultipleCopy.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Precision.hxx>
#endif

#include "Gui/Control.h"
#include <Mod/Fem/App/FemConstraintFluidBoundary.h>

#include "FemGuiTools.h"
#include "TaskFemConstraintFluidBoundary.h"
#include "ViewProviderFemConstraintFluidBoundary.h"


using namespace FemGui;

PROPERTY_SOURCE(FemGui::ViewProviderFemConstraintFluidBoundary,
                FemGui::ViewProviderFemConstraintOnBoundary)


ViewProviderFemConstraintFluidBoundary::ViewProviderFemConstraintFluidBoundary()
{
    sPixmap = "FEM_ConstraintFluidBoundary";
}

ViewProviderFemConstraintFluidBoundary::~ViewProviderFemConstraintFluidBoundary() = default;

bool ViewProviderFemConstraintFluidBoundary::setEdit(int ModNum)
{
    if (ModNum == ViewProvider::Default) {
        Gui::Control().closeDialog();
        // clear the selection (convenience)
        Gui::Selection().clearSelection();
        Gui::Control().showDialog(new TaskDlgFemConstraintFluidBoundary(this));

        return true;
    }
    else {
        return ViewProviderFemConstraintOnBoundary::setEdit(ModNum);
    }
}

// Rendering: Combination of ConstraintFixed and ConstraintForce
#define ARROWLENGTH (4)
#define ARROWHEADRADIUS (ARROWLENGTH / 3.0f)
#define WIDTH (2)
#define HEIGHT (1)
// #define USE_MULTIPLE_COPY  //OvG: MULTICOPY fails to update scaled display on initial drawing -
// so disable

void ViewProviderFemConstraintFluidBoundary::updateData(const App::Property* prop)
{
    // Gets called whenever a property of the attached object changes
    Fem::ConstraintFluidBoundary* pcConstraint = this->getObject<Fem::ConstraintFluidBoundary>();
    float scaledwidth =
        WIDTH * pcConstraint->Scale.getValue();  // OvG: Calculate scaled values once only
    float scaledheight = HEIGHT * pcConstraint->Scale.getValue();

    float scaledheadradius =
        ARROWHEADRADIUS * pcConstraint->Scale.getValue();  // OvG: Calculate scaled values once only
    float scaledlength = ARROWLENGTH * pcConstraint->Scale.getValue();

    std::string boundaryType = pcConstraint->BoundaryType.getValueAsString();
    if (prop == &pcConstraint->BoundaryType) {
        if (boundaryType == "wall") {
            ShapeAppearance.setDiffuseColor(0.0, 1.0, 1.0);
        }
        else if (boundaryType == "interface") {
            ShapeAppearance.setDiffuseColor(0.0, 1.0, 0.0);
        }
        else if (boundaryType == "freestream") {
            ShapeAppearance.setDiffuseColor(1.0, 1.0, 0.0);
        }
        else if (boundaryType == "inlet") {
            ShapeAppearance.setDiffuseColor(1.0, 0.0, 0.0);
        }
        else {  //(boundaryType == "outlet")
            ShapeAppearance.setDiffuseColor(0.0, 0.0, 1.0);
        }
    }

    if (boundaryType == "inlet" || boundaryType == "outlet") {
#ifdef USE_MULTIPLE_COPY
        // OvG: need access to cp for scaling
        SoMultipleCopy* cp = new SoMultipleCopy();
        if (pShapeSep->getNumChildren() == 0) {
            // Set up the nodes
            cp->matrix.setNum(0);
            cp->addChild(
                (SoNode*)GuiTools::createArrow(scaledlength, scaledheadradius));  // OvG: Scaling
            pShapeSep->addChild(cp);
        }
#endif

        if (prop == &pcConstraint->Points) {
            const std::vector<Base::Vector3d>& points = pcConstraint->Points.getValues();

#ifdef USE_MULTIPLE_COPY
            cp = static_cast<SoMultipleCopy*>(pShapeSep->getChild(0));
            cp->matrix.setNum(points.size());
            SbMatrix* matrices = cp->matrix.startEditing();
            int idx = 0;
#else
            // Redraw all arrows
            Gui::coinRemoveAllChildren(pShapeSep);
#endif
            // This should always point outside of the solid
            Base::Vector3d normal = pcConstraint->NormalDirection.getValue();

            // Get default direction (on first call to method)
            Base::Vector3d forceDirection = pcConstraint->DirectionVector.getValue();
            if (forceDirection.Length() < Precision::Confusion()) {
                forceDirection = normal;
            }

            SbVec3f dir(forceDirection.x, forceDirection.y, forceDirection.z);
            SbRotation rot(SbVec3f(0, 1, 0), dir);

            for (const auto& point : points) {
                SbVec3f base(point.x, point.y, point.z);
                if (forceDirection.GetAngle(normal) < std::numbers::pi
                        / 2) {  // Move arrow so it doesn't disappear inside the solid
                    base = base + dir * scaledlength;  // OvG: Scaling
                }
#ifdef USE_MULTIPLE_COPY
                SbMatrix m;
                m.setTransform(base, rot, SbVec3f(1, 1, 1));
                matrices[idx] = m;
                idx++;
#else
                SoSeparator* sep = new SoSeparator();
                GuiTools::createPlacement(sep, base, rot);
                GuiTools::createArrow(sep, scaledlength, scaledheadradius);  // OvG: Scaling
                pShapeSep->addChild(sep);
#endif
            }
#ifdef USE_MULTIPLE_COPY
            cp->matrix.finishEditing();
#endif
        }
        else if (prop == &pcConstraint->DirectionVector) {
            // Note: "Reversed" also triggers "DirectionVector"
            // Re-orient all arrows
            Base::Vector3d normal = pcConstraint->NormalDirection.getValue();
            Base::Vector3d forceDirection = pcConstraint->DirectionVector.getValue();
            if (forceDirection.Length() < Precision::Confusion()) {
                forceDirection = normal;
                if (boundaryType == "inlet") {
                    forceDirection = -normal;
                }
            }

            SbVec3f dir(forceDirection.x, forceDirection.y, forceDirection.z);
            SbRotation rot(SbVec3f(0, 1, 0), dir);

            const std::vector<Base::Vector3d>& points = pcConstraint->Points.getValues();

#ifdef USE_MULTIPLE_COPY
            SoMultipleCopy* cp = static_cast<SoMultipleCopy*>(pShapeSep->getChild(0));
            cp->matrix.setNum(points.size());
            SbMatrix* matrices = cp->matrix.startEditing();
#endif
            int idx = 0;

            for (const auto& point : points) {
                SbVec3f base(point.x, point.y, point.z);
                if (forceDirection.GetAngle(normal) < std::numbers::pi / 2) {
                    base = base + dir * scaledlength;  // OvG: Scaling
                }
#ifdef USE_MULTIPLE_COPY
                SbMatrix m;
                m.setTransform(base, rot, SbVec3f(1, 1, 1));
                matrices[idx] = m;
#else
                SoSeparator* sep = static_cast<SoSeparator*>(pShapeSep->getChild(idx));
                GuiTools::updatePlacement(sep, 0, base, rot);
                GuiTools::updateArrow(sep, 2, scaledlength, scaledheadradius);  // OvG: Scaling
#endif
                idx++;
            }
#ifdef USE_MULTIPLE_COPY
            cp->matrix.finishEditing();
#endif
        }
    }
    else {  // not inlet or outlet boundary type

#ifdef USE_MULTIPLE_COPY
        // OvG: always need access to cp for scaling
        SoMultipleCopy* cp = new SoMultipleCopy();
        if (pShapeSep->getNumChildren() == 0) {
            // Set up the nodes
            cp->matrix.setNum(0);
            cp->addChild(
                (SoNode*)GuiTools::createFixed(scaledheight, scaledwidth));  // OvG: Scaling
            pShapeSep->addChild(cp);
        }
#endif

        if (prop == &pcConstraint->Points) {
            const std::vector<Base::Vector3d>& points = pcConstraint->Points.getValues();
            const std::vector<Base::Vector3d>& normals = pcConstraint->Normals.getValues();
            if (points.size() != normals.size()) {
                return;
            }
            std::vector<Base::Vector3d>::const_iterator n = normals.begin();

#ifdef USE_MULTIPLE_COPY
            cp = static_cast<SoMultipleCopy*>(pShapeSep->getChild(0));
            cp->matrix.setNum(points.size());
            SbMatrix* matrices = cp->matrix.startEditing();
            int idx = 0;
#else
            // Note: Points and Normals are always updated together
            Gui::coinRemoveAllChildren(pShapeSep);
#endif

            for (const auto& point : points) {
                SbVec3f base(point.x, point.y, point.z);
                SbVec3f dir(n->x, n->y, n->z);
                SbRotation rot(SbVec3f(0, -1, 0), dir);
#ifdef USE_MULTIPLE_COPY
                SbMatrix m;
                m.setTransform(base, rot, SbVec3f(1, 1, 1));
                matrices[idx] = m;
                idx++;
#else
                SoSeparator* sep = new SoSeparator();
                GuiTools::createPlacement(sep, base, rot);
                GuiTools::createFixed(sep, scaledheight, scaledwidth);  // OvG: Scaling
                pShapeSep->addChild(sep);
#endif
                n++;
            }
#ifdef USE_MULTIPLE_COPY
            cp->matrix.finishEditing();
#endif
        }
    }

    ViewProviderFemConstraint::updateData(prop);
}
