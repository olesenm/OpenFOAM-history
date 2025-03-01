/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2013-2014 OpenFOAM Foundation
     \\/     M anipulation  |
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM.  If not, see <http://www.gnu.org/licenses/>.

Class
    Foam::dynamicMotionSolverTopoFvMesh

Description
    Mesh motion solver which combines both topology change and mesh motion

SourceFiles
    dynamicMotionSolverTopoFvMesh.C

\*---------------------------------------------------------------------------*/

#ifndef dynamicMotionSolverTopoFvMesh_H
#define dynamicMotionSolverTopoFvMesh_H

#include "topoChangerFvMesh.H"
#include "motionSolver.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

/*---------------------------------------------------------------------------*\
                Class dynamicMotionSolverTopoFvMesh Declaration
\*---------------------------------------------------------------------------*/

class dynamicMotionSolverTopoFvMesh
:
    public topoChangerFvMesh
{
    // Private data

        //- Motion solver
        autoPtr<motionSolver> motionPtr_;


    // Private Member Functions

        //- Disallow default bitwise copy construct
        dynamicMotionSolverTopoFvMesh(const dynamicMotionSolverTopoFvMesh&);

        //- Disallow default bitwise assignment
        void operator=(const dynamicMotionSolverTopoFvMesh&);


public:

    //- Runtime type information
    TypeName("dynamicMotionSolverTopoFvMesh");


    // Constructors

        //- Construct from database
        dynamicMotionSolverTopoFvMesh(const IOobject& io);


    //- Destructor
    virtual ~dynamicMotionSolverTopoFvMesh();


    // Member Functions

        //- Update the mesh for both mesh motion and topology change
        virtual bool update();
};


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
