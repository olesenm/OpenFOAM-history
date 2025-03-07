#----------------------------------*-sh-*--------------------------------------
# =========                 |
# \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
#  \\    /   O peration     |
#   \\  /    A nd           | Copyright (C) 2011-2015 OpenFOAM Foundation
#    \\/     M anipulation  |
#------------------------------------------------------------------------------
#     Copyright (C) 2008-2011 Mark Olesen
#------------------------------------------------------------------------------
# License
#     This file is part of OpenFOAM.
#
#     OpenFOAM is free software: you can redistribute it and/or modify it
#     under the terms of the GNU General Public License as published by
#     the Free Software Foundation, either version 3 of the License, or
#     (at your option) any later version.
#
#     OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
#     ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
#     FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
#     for more details.
#
#     You should have received a copy of the GNU General Public License
#     along with OpenFOAM.  If not, see <http://www.gnu.org/licenses/>.
#
# File
#     config/paraview.sh
#
# Description
#     Setup file for paraview-[3-4].x
#     Sourced from OpenFOAM-<VERSION>/etc/bashrc or from foamPV alias
#
# Note
#     The env. variables 'ParaView_DIR' and 'ParaView_MAJOR'
#     are required for building plugins
#------------------------------------------------------------------------------

# clean the PATH
cleaned=`$WM_PROJECT_DIR/bin/foamCleanPath "$PATH" "$WM_THIRD_PARTY_DIR/platforms/$WM_ARCH$WM_COMPILER/cmake- $WM_THIRD_PARTY_DIR/platforms/$WM_ARCH$WM_COMPILER/paraview-"` && PATH="$cleaned"

# determine the cmake to be used
unset CMAKE_HOME
for cmake in cmake-3.2.1 cmake-2.8.12.1 cmake-2.8.8 cmake-2.8.4 cmake-2.8.3 cmake-2.8.1
do
    cmake=$WM_THIRD_PARTY_DIR/platforms/$WM_ARCH$WM_COMPILER/$cmake
    if [ -r $cmake ]
    then
        export CMAKE_HOME=$cmake
        export PATH=$CMAKE_HOME/bin:$PATH
        break
    fi
done


#- ParaView version, automatically determine major version
#export ParaView_VERSION=3.12.0
#export ParaView_VERSION=4.0.1
#export ParaView_VERSION=4.1.0
export ParaView_VERSION=4.3.1
export ParaView_MAJOR=detect


# Evaluate command-line parameters for ParaView
_foamParaviewEval()
{
    while [ $# -gt 0 ]
    do
        case "$1" in
        ParaView*=*)
            # name=value  -> export name=value
            eval "export $1"
            ;;
        esac
        shift
    done
}

# Evaluate command-line parameters
_foamParaviewEval $@


# set MAJOR version to correspond to VERSION
# ParaView_MAJOR is "<digits>.<digits>" from ParaView_VERSION
case "$ParaView_VERSION" in
"$ParaView_MAJOR".* )
    # version and major appear to correspond
    ;;

[0-9]*)
    # extract major from the version
    ParaView_MAJOR=`echo $ParaView_VERSION | sed -e 's/^\([0-9][0-9]*\.[0-9][0-9]*\).*$/\1/'`
    ;;
esac
export ParaView_VERSION ParaView_MAJOR

paraviewInstDir=$WM_THIRD_PARTY_DIR/ParaView-$ParaView_VERSION
paraviewArchName=ParaView-$ParaView_VERSION

export ParaView_DIR=$WM_THIRD_PARTY_DIR/platforms/$WM_ARCH$WM_COMPILER/$paraviewArchName

# set paths if binaries or source are present
if [ -r $ParaView_DIR -o -r $paraviewInstDir ]
then
    export ParaView_INCLUDE_DIR=$ParaView_DIR/include/paraview-$ParaView_MAJOR
    if [ ! -d $ParaView_INCLUDE_DIR -a -d $ParaView_DIR/include/paraview ]
    then
        export ParaView_INCLUDE_DIR=$ParaView_DIR/include/paraview
    fi

    ParaView_LIB_DIR=$ParaView_DIR/lib/paraview-$ParaView_MAJOR
    if [ ! -d $ParaView_LIB_DIR -a -d $ParaView_DIR/lib/paraview ]
    then
        ParaView_LIB_DIR=$ParaView_DIR/lib/paraview
    fi

    export PATH=$ParaView_DIR/bin:$PATH
    export LD_LIBRARY_PATH=$ParaView_LIB_DIR:$LD_LIBRARY_PATH
    export PV_PLUGIN_PATH=$FOAM_LIBBIN/paraview-$ParaView_MAJOR

    if [ "$FOAM_VERBOSE" -a "$PS1" ]
    then
        echo "Using paraview"
        echo "    ParaView_DIR         : $ParaView_DIR"
        echo "    ParaView_LIB_DIR     : $ParaView_LIB_DIR"
        echo "    ParaView_INCLUDE_DIR : $ParaView_INCLUDE_DIR"
        echo "    PV_PLUGIN_PATH       : $PV_PLUGIN_PATH"
    fi

    # add in python libraries if required
    paraviewPython=$ParaView_DIR/Utilities/VTKPythonWrapping
    if [ -r $paraviewPython ]
    then
        if [ "$PYTHONPATH" ]
        then
            export PYTHONPATH=$PYTHONPATH:$paraviewPython:$ParaView_LIB_DIR
        else
            export PYTHONPATH=$paraviewPython:$ParaView_LIB_DIR
        fi
    fi
else
    unset PV_PLUGIN_PATH
fi

unset _foamParaviewEval
unset cleaned cmake paraviewInstDir paraviewPython

# -----------------------------------------------------------------------------
