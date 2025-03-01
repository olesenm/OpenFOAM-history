/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2011-2014 OpenFOAM Foundation
     \\/     M anipulation  |
-------------------------------------------------------------------------------
    Copyright (C) 2011 Mark Olesen
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

\*---------------------------------------------------------------------------*/

#include "ensightSurfaceWriter.H"
#include "ensightPartFaces.H"
#include "makeSurfaceWriterMethods.H"

// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

namespace Foam
{
    makeSurfaceWriterType(ensightSurfaceWriter);
    addToRunTimeSelectionTable(surfaceWriter, ensightSurfaceWriter, wordDict);
}


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::ensightSurfaceWriter::ensightSurfaceWriter()
:
    surfaceWriter(),
    writeFormat_(IOstream::ASCII),
    collateTimes_(false)
{}


Foam::ensightSurfaceWriter::ensightSurfaceWriter(const dictionary& options)
:
    surfaceWriter(),
    writeFormat_(IOstream::ASCII)
{
    // choose ascii or binary format
    if (options.found("format"))
    {
        writeFormat_ = IOstream::formatEnum(options.lookup("format"));
    }
    options.readIfPresent("collateTimes", collateTimes_);
}


// * * * * * * * * * * * * * * * * Destructor  * * * * * * * * * * * * * * * //

Foam::ensightSurfaceWriter::~ensightSurfaceWriter()
{}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

Foam::fileName Foam::ensightSurfaceWriter::write
(
    const fileName& outputDir,
    const fileName& surfaceName,
    const pointField& points,
    const faceList& faces,
    const bool verbose
) const
{
    if (!isDir(outputDir))
    {
        mkDir(outputDir);
    }

    // const scalar timeValue = Foam::name(this->mesh().time().timeValue());
    const scalar timeValue = 0.0;

    OFstream osCase(outputDir/surfaceName + ".case");
    ensightGeoFile osGeom
    (
        outputDir/surfaceName + ".0000.mesh",
        writeFormat_
    );

    if (verbose)
    {
        Info<< "Writing case file to " << osCase.name() << endl;
    }

    osCase
        << "FORMAT" << nl
        << "type: ensight gold" << nl
        << nl
        << "GEOMETRY" << nl
        << "model:        1     " << osGeom.name().name() << nl
        << nl
        << "TIME" << nl
        << "time set:                      1" << nl
        << "number of steps:               1" << nl
        << "filename start number:         0" << nl
        << "filename increment:            1" << nl
        << "time values:" << nl
        << timeValue << nl
        << nl;

    ensightPartFaces ensPart(0, osGeom.name().name(), points, faces, true);
    osGeom << ensPart;

    return osCase.name();
}


// create write methods
defineSurfaceWriterWriteFields(Foam::ensightSurfaceWriter);


// ************************************************************************* //
