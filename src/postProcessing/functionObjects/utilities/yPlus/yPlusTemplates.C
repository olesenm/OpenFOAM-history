/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2013-2015 OpenFOAM Foundation
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

\*---------------------------------------------------------------------------*/

#include "wallFvPatch.H"
#include "nutWallFunctionFvPatchScalarField.H"
#include "nearWallDist.H"

template<class TurbulenceModel>
void Foam::yPlus::calcYPlus
(
    const TurbulenceModel& turbulenceModel,
    const fvMesh& mesh,
    const volVectorField& U,
    volScalarField& yPlus
)
{
    volScalarField::GeometricBoundaryField d = nearWallDist(mesh).y();

    const volScalarField::GeometricBoundaryField nutBf =
        turbulenceModel.nut()().boundaryField();

    const volScalarField::GeometricBoundaryField nuEffBf =
        turbulenceModel.nuEff()().boundaryField();

    const volScalarField::GeometricBoundaryField nuBf =
        turbulenceModel.nu()().boundaryField();

    const fvPatchList& patches = mesh.boundary();

    forAll(patches, patchi)
    {
        const fvPatch& patch = patches[patchi];

        if (isA<nutWallFunctionFvPatchScalarField>(nutBf[patchi]))
        {
            const nutWallFunctionFvPatchScalarField& nutPf =
                dynamic_cast<const nutWallFunctionFvPatchScalarField&>
                (
                    nutBf[patchi]
                );

            yPlus.boundaryField()[patchi] = nutPf.yPlus();
            const scalarField& Yp = yPlus.boundaryField()[patchi];

            const scalar minYp = gMin(Yp);
            const scalar maxYp = gMax(Yp);
            const scalar avgYp = gAverage(Yp);

            Info(log_)
                << "    patch " << patch.name()
                << " y+ : min = " << minYp << ", max = " << maxYp
                << ", average = " << avgYp << nl;

            file() << obr_.time().value()
                << token::TAB << patch.name()
                << token::TAB << minYp
                << token::TAB << maxYp
                << token::TAB << avgYp
                << endl;
        }
        else if (isA<wallFvPatch>(patch))
        {
            yPlus.boundaryField()[patchi] =
                d[patchi]
               *sqrt
                (
                    nuEffBf[patchi]
                   *mag(U.boundaryField()[patchi].snGrad())
                )/nuBf[patchi];
            const scalarField& Yp = yPlus.boundaryField()[patchi];

            const scalar minYp = gMin(Yp);
            const scalar maxYp = gMax(Yp);
            const scalar avgYp = gAverage(Yp);

            Info(log_)
                << "    patch " << patch.name()
                << " y+ : min = " << minYp << ", max = " << maxYp
                << ", average = " << avgYp << nl;

            file() << obr_.time().value()
                << token::TAB << patch.name()
                << token::TAB << minYp
                << token::TAB << maxYp
                << token::TAB << avgYp
                << endl;
        }
    }
}


// ************************************************************************* //
