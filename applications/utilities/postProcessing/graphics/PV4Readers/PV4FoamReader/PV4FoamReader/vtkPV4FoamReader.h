/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2011-2013 OpenFOAM Foundation
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

Class
    vtkPV4FoamReader

Description
    reads a dataset in OpenFOAM format

    vtkPV4blockMeshReader creates an multiblock dataset.
    It uses the OpenFOAM infrastructure (fvMesh, etc) to handle mesh and
    field data.

SourceFiles
    vtkPV4blockMeshReader.cxx

\*---------------------------------------------------------------------------*/
#ifndef vtkPV4FoamReader_h
#define vtkPV4FoamReader_h

// VTK includes
#include "vtkMultiBlockDataSetAlgorithm.h"

// * * * * * * * * * * * * * Forward Declarations  * * * * * * * * * * * * * //

// VTK forward declarations
class vtkDataArraySelection;
class vtkCallbackCommand;

// OpenFOAM forward declarations
namespace Foam
{
    class vtkPV4Foam;
}


/*---------------------------------------------------------------------------*\
                     Class vtkPV4FoamReader Declaration
\*---------------------------------------------------------------------------*/

class vtkPV4FoamReader
:
    public vtkMultiBlockDataSetAlgorithm
{
public:
    vtkTypeMacro(vtkPV4FoamReader, vtkMultiBlockDataSetAlgorithm);
    void PrintSelf(ostream&, vtkIndent);

    static vtkPV4FoamReader* New();

    // Description:
    // Get the current timestep and the timestep range.
    vtkGetVector2Macro(TimeStepRange, int);

    // Description:
    // Set/Get the filename.
    vtkSetStringMacro(FileName);
    vtkGetStringMacro(FileName);

    // Description:
    // OpenFOAM mesh caching control
    vtkSetMacro(CacheMesh, int);
    vtkGetMacro(CacheMesh, int);

    // Description:
    // OpenFOAM refresh times/fields
    virtual void SetRefresh(int);

    // Description:
    // OpenFOAM skip/include the 0/ time directory
    vtkSetMacro(SkipZeroTime, int);
    vtkGetMacro(SkipZeroTime, int);

    // Description:
    // GUI update control
    vtkSetMacro(UpdateGUI, int);
    vtkGetMacro(UpdateGUI, int);

    // Description:
    // OpenFOAM extrapolate internal values onto the patches
    vtkSetMacro(ExtrapolatePatches, int);
    vtkGetMacro(ExtrapolatePatches, int);

    // Description:
    // OpenFOAM use vtkPolyhedron instead of decomposing polyhedra
    vtkSetMacro(UseVTKPolyhedron, int);
    vtkGetMacro(UseVTKPolyhedron, int);

    // Description:
    // OpenFOAM read sets control
    virtual void SetIncludeSets(int);
    vtkGetMacro(IncludeSets, int);

    // Description:
    // OpenFOAM read zones control
    virtual void SetIncludeZones(int);
    vtkGetMacro(IncludeZones, int);

    // Description:
    // OpenFOAM display patch names control
    virtual void SetShowPatchNames(int);
    vtkGetMacro(ShowPatchNames, int);

    // Description:
    // OpenFOAM display patchGroups
    virtual void SetShowGroupsOnly(int);
    vtkGetMacro(ShowGroupsOnly, int);

    // Description:
    // OpenFOAM volField interpolation
    vtkSetMacro(InterpolateVolFields, int);
    vtkGetMacro(InterpolateVolFields, int);

    // Description:
    // Get the current timestep
    int  GetTimeStep();

    // Description:
    // Parts selection list control
    virtual vtkDataArraySelection* GetPartSelection();
    int  GetNumberOfPartArrays();
    int  GetPartArrayStatus(const char* name);
    void SetPartArrayStatus(const char* name, int status);
    const char* GetPartArrayName(int index);

    // Description:
    // volField selection list control
    virtual vtkDataArraySelection* GetVolFieldSelection();
    int  GetNumberOfVolFieldArrays();
    int  GetVolFieldArrayStatus(const char* name);
    void SetVolFieldArrayStatus(const char* name, int status);
    const char* GetVolFieldArrayName(int index);

    // Description:
    // pointField selection list control
    virtual vtkDataArraySelection* GetPointFieldSelection();
    int  GetNumberOfPointFieldArrays();
    int  GetPointFieldArrayStatus(const char* name);
    void SetPointFieldArrayStatus(const char* name, int status);
    const char* GetPointFieldArrayName(int index);

    // Description:
    // lagrangianField selection list control
    virtual vtkDataArraySelection* GetLagrangianFieldSelection();
    int  GetNumberOfLagrangianFieldArrays();
    int  GetLagrangianFieldArrayStatus(const char* name);
    void SetLagrangianFieldArrayStatus(const char* name, int status);
    const char* GetLagrangianFieldArrayName(int index);

    // Description:
    // Callback registered with the SelectionObserver
    // for all the selection lists
    static void SelectionModifiedCallback
    (
        vtkObject* caller,
        unsigned long eid,
        void* clientdata,
        void* calldata
    );

    void SelectionModified();


protected:

    //- Construct null
    vtkPV4FoamReader();

    //- Destructor
    ~vtkPV4FoamReader();

    //- Return information about mesh, times, etc without loading anything
    virtual int RequestInformation
    (
        vtkInformation*,
        vtkInformationVector**,
        vtkInformationVector*
    );

    //- Get the mesh/fields for a particular time
    virtual int RequestData
    (
        vtkInformation*,
        vtkInformationVector**,
        vtkInformationVector*
    );

    //- Fill in additional port information
    virtual int FillOutputPortInformation(int, vtkInformation*);

    //- The observer to modify this object when array selections are modified
    vtkCallbackCommand* SelectionObserver;

    //- The file name for this case
    char* FileName;


private:

    //- Disallow default bitwise copy construct
    vtkPV4FoamReader(const vtkPV4FoamReader&);

    //- Disallow default bitwise assignment
    void operator=(const vtkPV4FoamReader&);

    //- Add/remove patch names to/from the view
    void updatePatchNamesView(const bool show);

    int TimeStepRange[2];
    int Refresh;
    int CacheMesh;
    int SkipZeroTime;

    int ExtrapolatePatches;
    int UseVTKPolyhedron;
    int IncludeSets;
    int IncludeZones;
    int ShowPatchNames;
    int ShowGroupsOnly;
    int InterpolateVolFields;

    //- Dummy variable/switch to invoke a reader update
    int UpdateGUI;

    vtkDataArraySelection* PartSelection;
    vtkDataArraySelection* VolFieldSelection;
    vtkDataArraySelection* PointFieldSelection;
    vtkDataArraySelection* LagrangianFieldSelection;

    //- Cached data for output port0 (experimental!)
    vtkMultiBlockDataSet* output0_;

    //BTX
    Foam::vtkPV4Foam* foamData_;
    //ETX
};

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
