/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2011-2014 OpenFOAM Foundation
     \\/     M anipulation  |
-------------------------------------------------------------------------------
    Copyright (C) 2009-2011 Mark Olesen
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
    Foam::fileName

Description
    A class for handling file names.

    A fileName is a string of characters without whitespace or quotes.
    A fileName can be
      - constructed from a char*, a string or a word
      - concatenated by adding a '/' separator
      - decomposed into the path, name or component list
      - interrogated for type and access mode

    The string::expand() method expands environment variables, etc,

SourceFiles
    fileName.C
    fileNameIO.C

\*---------------------------------------------------------------------------*/

#ifndef fileName_H
#define fileName_H

#include "word.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

template<class T> class List;
typedef List<word> wordList;

// Forward declaration of friend functions and operators

class fileName;

Istream& operator>>(Istream&, fileName&);
Ostream& operator<<(Ostream&, const fileName&);


/*---------------------------------------------------------------------------*\
                          Class fileName Declaration
\*---------------------------------------------------------------------------*/

class fileName
:
    public string
{
    // Private Member Functions

        //- Strip invalid characters
        inline void stripInvalid();


public:

    //- Enumerations to handle file types and modes.
    enum Type
    {
        UNDEFINED,
        FILE,
        DIRECTORY,
        LINK
    };


    // Static data members

        static const char* const typeName;
        static int debug;

        //- An empty fileName
        static const fileName null;


    // Constructors

        //- Construct null
        inline fileName();

        //- Construct as copy
        inline fileName(const fileName&);

        //- Construct as copy of word
        inline fileName(const word&);

        //- Construct as copy of string
        inline fileName(const string&);

        //- Construct as copy of std::string
        inline fileName(const std::string&);

        //- Construct as copy of character array
        inline fileName(const char*);

        //- Construct by concatenating elements of wordList separated by '/'
        explicit fileName(const wordList&);

        //- Construct from Istream
        fileName(Istream&);


    // Member functions

        //- Is this character valid for a fileName?
        inline static bool valid(char);

        //- Cleanup file name
        //  eg, remove repeated slashes, etc.
        bool clean();

        //- Cleanup file name
        //  eg, remove repeated slashes, etc.
        fileName clean() const;


        // Interrogation

            //- Return the file type: FILE, DIRECTORY, UNDEFINED or
            //  LINK (only if followLink=false)
            Type type(const bool followLink = true) const;

            //- Return true if file name is absolute
            bool isAbsolute() const;

            //- Convert from relative to absolute
            fileName& toAbsolute();


        // Decomposition

            //- Return file name (part beyond last /)
            word name() const;

            //- Return file name (part beyond last /), subsitute for FOAM_CASE
            string caseName() const;

            //- Return file name, optionally without extension
            word name(const bool noExt) const;

            //- Return directory path name (part before last /)
            fileName path() const;

            //- Return file name without extension (part before last .)
            fileName lessExt() const;

            //- Return file name extension (part after last .)
            word ext() const;

            //- Return path components as wordList
            wordList components(const char delimiter='/') const;

            //- Return a single component of the path
            word component(const size_type, const char delimiter='/') const;


    // Member operators

        // Assignment

            const fileName& operator=(const fileName&);
            const fileName& operator=(const word&);
            const fileName& operator=(const string&);
            const fileName& operator=(const std::string&);
            const fileName& operator=(const char*);


    // IOstream operators

        friend Istream& operator>>(Istream&, fileName&);
        friend Ostream& operator<<(Ostream&, const fileName&);
};


//- Assemble words and fileNames as pathnames by adding a '/' separator
fileName operator/(const string&, const string&);


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#include "fileNameI.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
