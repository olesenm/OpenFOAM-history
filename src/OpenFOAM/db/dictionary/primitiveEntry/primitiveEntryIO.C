/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2011-2012 OpenFOAM Foundation
     \\/     M anipulation  |
-------------------------------------------------------------------------------
    Copyright (C) 2008-2011 Mark Olesen
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

Description
    PrimitiveEntry constructor from Istream and Ostream output operator.

\*---------------------------------------------------------------------------*/

#include "primitiveEntry.H"
#include "functionEntry.H"

// * * * * * * * * * * * * * Private Member Functions  * * * * * * * * * * * //

void Foam::primitiveEntry::append
(
    const token& currToken,
    const dictionary& dict,
    Istream& is
)
{
    if (currToken.isWord())
    {
        const word& w = currToken.wordToken();

        if
        (
            disableFunctionEntries
         || w.size() == 1
         || (
                !(w[0] == '$' && expandVariable(w, dict))
             && !(w[0] == '#' && expandFunction(w, dict, is))
            )
        )
        {
            newElmt(tokenIndex()++) = currToken;
        }
    }
    else if (currToken.isVariable())
    {
        const string& w = currToken.stringToken();

        if
        (
            disableFunctionEntries
         || w.size() <= 3
         || !(
                w[0] == '$'
             && w[1] == token::BEGIN_BLOCK
             && expandVariable(w, dict)
            )
        )
        {
            newElmt(tokenIndex()++) = currToken;
        }
    }
    else
    {
        newElmt(tokenIndex()++) = currToken;
    }
}


bool Foam::primitiveEntry::expandFunction
(
    const word& keyword,
    const dictionary& parentDict,
    Istream& is
)
{
    word functionName = keyword(1, keyword.size()-1);
    return functionEntry::execute(functionName, parentDict, *this, is);
}


bool Foam::primitiveEntry::read(const dictionary& dict, Istream& is)
{
    is.fatalCheck
    (
        "primitiveEntry::readData(const dictionary&, Istream&)"
    );

    label blockCount = 0;
    token currToken;

    if
    (
        !is.read(currToken).bad()
     && currToken.good()
     && currToken != token::END_STATEMENT
    )
    {
        append(currToken, dict, is);

        if
        (
            currToken == token::BEGIN_BLOCK
         || currToken == token::BEGIN_LIST
        )
        {
            blockCount++;
        }

        while
        (
            !is.read(currToken).bad()
         && currToken.good()
         && !(currToken == token::END_STATEMENT && blockCount == 0)
        )
        {
            if
            (
                currToken == token::BEGIN_BLOCK
             || currToken == token::BEGIN_LIST
            )
            {
                blockCount++;
            }
            else if
            (
                currToken == token::END_BLOCK
             || currToken == token::END_LIST
            )
            {
                blockCount--;
            }

            append(currToken, dict, is);
        }
    }

    is.fatalCheck
    (
        "primitiveEntry::readData(const dictionary&, Istream&)"
    );

    if (currToken.good())
    {
        return true;
    }
    else
    {
        return false;
    }
}


void Foam::primitiveEntry::readEntry(const dictionary& dict, Istream& is)
{
    label keywordLineNumber = is.lineNumber();
    tokenIndex() = 0;

    if (read(dict, is))
    {
        setSize(tokenIndex());
        tokenIndex() = 0;
    }
    else
    {
        std::ostringstream os;
        os  << "ill defined primitiveEntry starting at keyword '"
            << keyword() << '\''
            << " on line " << keywordLineNumber
            << " and ending at line " << is.lineNumber();

        SafeFatalIOErrorIn
        (
            "primitiveEntry::readEntry(const dictionary&, Istream&)",
            is,
            os.str()
        );
    }
}


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::primitiveEntry::primitiveEntry
(
    const keyType& key,
    const dictionary& dict,
    Istream& is
)
:
    entry(key),
    ITstream
    (
        is.name() + '.' + key,
        tokenList(10),
        is.format(),
        is.version()
    )
{
    readEntry(dict, is);
}


Foam::primitiveEntry::primitiveEntry(const keyType& key, Istream& is)
:
    entry(key),
    ITstream
    (
        is.name() + '.' + key,
        tokenList(10),
        is.format(),
        is.version()
    )
{
    readEntry(dictionary::null, is);
}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

void Foam::primitiveEntry::write(Ostream& os, const bool contentsOnly) const
{
    if (!contentsOnly)
    {
        os.writeKeyword(keyword());
    }

    for (label i=0; i<size(); ++i)
    {
        const token& t = operator[](i);
        if (t.type() == token::VERBATIMSTRING)
        {
            // Bypass token output operator to avoid losing verbatimness.
            // Handle in Ostreams themselves
            os.write(t);
        }
        else
        {
            os  << t;
        }

        if (i < size()-1)
        {
            os  << token::SPACE;
        }
    }

    if (!contentsOnly)
    {
        os  << token::END_STATEMENT << endl;
    }
}


void Foam::primitiveEntry::write(Ostream& os) const
{
    this->write(os, false);
}


// * * * * * * * * * * * * * Ostream operator  * * * * * * * * * * * * * * * //

template<>
Foam::Ostream& Foam::operator<<
(
    Ostream& os,
    const InfoProxy<primitiveEntry>& ip
)
{
    const primitiveEntry& e = ip.t_;

    e.print(os);

    const label nPrintTokens = 10;

    os  << "    primitiveEntry '" << e.keyword() << "' comprises ";

    for (label i=0; i<min(e.size(), nPrintTokens); i++)
    {
        os  << nl << "        " << e[i].info();
    }

    if (e.size() > nPrintTokens)
    {
        os  << " ...";
    }

    os  << endl;

    return os;
}


// ************************************************************************* //
