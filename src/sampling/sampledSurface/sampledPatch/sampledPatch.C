/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2011-2015 OpenFOAM Foundation
     \\/     M anipulation  |
-------------------------------------------------------------------------------
    Copyright (C) 2008-2010 Mark Olesen
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

#include "sampledPatch.H"
#include "dictionary.H"
#include "polyMesh.H"
#include "polyPatch.H"
#include "volFields.H"
#include "surfaceFields.H"

#include "addToRunTimeSelectionTable.H"

// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

namespace Foam
{
    defineTypeNameAndDebug(sampledPatch, 0);
    addNamedToRunTimeSelectionTable(sampledSurface, sampledPatch, word, patch);
}


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::sampledPatch::sampledPatch
(
    const word& name,
    const polyMesh& mesh,
    const wordReList& patchNames,
    const bool triangulate
)
:
    sampledSurface(name, mesh),
    patchNames_(patchNames),
    triangulate_(triangulate),
    needsUpdate_(true)
{}


Foam::sampledPatch::sampledPatch
(
    const word& name,
    const polyMesh& mesh,
    const dictionary& dict
)
:
    sampledSurface(name, mesh, dict),
    patchNames_(dict.lookup("patches")),
    triangulate_(dict.lookupOrDefault("triangulate", false)),
    needsUpdate_(true)
{}


// * * * * * * * * * * * * * * * * Destructor  * * * * * * * * * * * * * * * //

Foam::sampledPatch::~sampledPatch()
{}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

const Foam::labelList& Foam::sampledPatch::patchIDs() const
{
    if (patchIDs_.empty())
    {
        patchIDs_ = mesh().boundaryMesh().patchSet(patchNames_).sortedToc();
    }
    return patchIDs_;
}


bool Foam::sampledPatch::needsUpdate() const
{
    return needsUpdate_;
}


bool Foam::sampledPatch::expire()
{
    // already marked as expired
    if (needsUpdate_)
    {
        return false;
    }

    sampledSurface::clearGeom();
    MeshStorage::clear();
    patchIDs_.clear();
    patchIndex_.clear();
    patchFaceLabels_.clear();
    patchStart_.clear();

    needsUpdate_ = true;
    return true;
}


bool Foam::sampledPatch::update()
{
    if (!needsUpdate_)
    {
        return false;
    }

    label sz = 0;
    forAll(patchIDs(), i)
    {
        label patchI = patchIDs()[i];
        const polyPatch& pp = mesh().boundaryMesh()[patchI];

        if (isA<emptyPolyPatch>(pp))
        {
            FatalErrorIn("sampledPatch::update()")
                << "Cannot sample an empty patch. Patch " << pp.name()
                << exit(FatalError);
        }

        sz += pp.size();
    }

    // For every face (or triangle) the originating patch and local face in the
    // patch.
    patchIndex_.setSize(sz);
    patchFaceLabels_.setSize(sz);
    patchStart_.setSize(patchIDs().size());
    labelList meshFaceLabels(sz);

    sz = 0;

    forAll(patchIDs(), i)
    {
        label patchI = patchIDs()[i];

        patchStart_[i] = sz;

        const polyPatch& pp = mesh().boundaryMesh()[patchI];

        forAll(pp, j)
        {
            patchIndex_[sz] = i;
            patchFaceLabels_[sz] = j;
            meshFaceLabels[sz] = pp.start()+j;
            sz++;
        }
    }

    indirectPrimitivePatch allPatches
    (
        IndirectList<face>(mesh().faces(), meshFaceLabels),
        mesh().points()
    );

    this->storedPoints() = allPatches.localPoints();
    this->storedFaces()  = allPatches.localFaces();


    // triangulate uses remapFaces()
    // - this is somewhat less efficient since it recopies the faces
    // that we just created, but we probably don't want to do this
    // too often anyhow.
    if (triangulate_)
    {
        MeshStorage::triangulate();
    }

    if (debug)
    {
        print(Pout);
        Pout<< endl;
    }

    needsUpdate_ = false;
    return true;
}


// remap action on triangulation
void Foam::sampledPatch::remapFaces(const labelUList& faceMap)
{
    // recalculate the cells cut
    if (notNull(faceMap) && faceMap.size())
    {
        MeshStorage::remapFaces(faceMap);
        patchFaceLabels_ = labelList
        (
            UIndirectList<label>(patchFaceLabels_, faceMap)
        );
        patchIndex_ = labelList
        (
            UIndirectList<label>(patchIndex_, faceMap)
        );

        // Redo patchStart.
        if (patchIndex_.size() > 0)
        {
            patchStart_[patchIndex_[0]] = 0;
            for (label i = 1; i < patchIndex_.size(); i++)
            {
                if (patchIndex_[i] != patchIndex_[i-1])
                {
                    patchStart_[patchIndex_[i]] = i;
                }
            }
        }
    }
}


Foam::tmp<Foam::scalarField> Foam::sampledPatch::sample
(
    const volScalarField& vField
) const
{
    return sampleField(vField);
}


Foam::tmp<Foam::vectorField> Foam::sampledPatch::sample
(
    const volVectorField& vField
) const
{
    return sampleField(vField);
}


Foam::tmp<Foam::sphericalTensorField> Foam::sampledPatch::sample
(
    const volSphericalTensorField& vField
) const
{
    return sampleField(vField);
}


Foam::tmp<Foam::symmTensorField> Foam::sampledPatch::sample
(
    const volSymmTensorField& vField
) const
{
    return sampleField(vField);
}


Foam::tmp<Foam::tensorField> Foam::sampledPatch::sample
(
    const volTensorField& vField
) const
{
    return sampleField(vField);
}


Foam::tmp<Foam::scalarField> Foam::sampledPatch::sample
(
    const surfaceScalarField& sField
) const
{
    return sampleField(sField);
}


Foam::tmp<Foam::vectorField> Foam::sampledPatch::sample
(
    const surfaceVectorField& sField
) const
{
    return sampleField(sField);
}


Foam::tmp<Foam::sphericalTensorField> Foam::sampledPatch::sample
(
    const surfaceSphericalTensorField& sField
) const
{
    return sampleField(sField);
}


Foam::tmp<Foam::symmTensorField> Foam::sampledPatch::sample
(
    const surfaceSymmTensorField& sField
) const
{
    return sampleField(sField);
}


Foam::tmp<Foam::tensorField> Foam::sampledPatch::sample
(
    const surfaceTensorField& sField
) const
{
    return sampleField(sField);
}


Foam::tmp<Foam::scalarField> Foam::sampledPatch::interpolate
(
    const interpolation<scalar>& interpolator
) const
{
    return interpolateField(interpolator);
}


Foam::tmp<Foam::vectorField> Foam::sampledPatch::interpolate
(
    const interpolation<vector>& interpolator
) const
{
    return interpolateField(interpolator);
}


Foam::tmp<Foam::sphericalTensorField> Foam::sampledPatch::interpolate
(
    const interpolation<sphericalTensor>& interpolator
) const
{
    return interpolateField(interpolator);
}


Foam::tmp<Foam::symmTensorField> Foam::sampledPatch::interpolate
(
    const interpolation<symmTensor>& interpolator
) const
{
    return interpolateField(interpolator);
}


Foam::tmp<Foam::tensorField> Foam::sampledPatch::interpolate
(
    const interpolation<tensor>& interpolator
) const
{
    return interpolateField(interpolator);
}


void Foam::sampledPatch::print(Ostream& os) const
{
    os  << "sampledPatch: " << name() << " :"
        << "  patches:" << patchNames()
        << "  faces:" << faces().size()
        << "  points:" << points().size();
}


// ************************************************************************* //
