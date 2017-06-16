/****************************************************************************
 * Copyright (c) 2012-2017 by the DataTransferKit authors                   *
 * All rights reserved.                                                     *
 *                                                                          *
 * This file is part of the DataTransferKit library. DataTransferKit is     *
 * distributed under a BSD 3-clause license. For the licensing terms see    *
 * the LICENSE file in the top-level directory.                             *
 ****************************************************************************/
#include <Kokkos_Core.hpp>

#include "DTK_Core.hpp"
#include "DTK_DBC.hpp"

namespace DataTransferKit
{
namespace
{ // anonymous

// Whether one of the Tpetra::initialize() functions has been called before.
bool dtkIsInitialized = false;

// Whether DTK initialized Kokkos. DTK's finalize() only finalizes
// Kokkos if it initialized Kokkos. Otherwise, something else
// initialized Kokkos and is responsible for finalizing it.
bool dtkInitializedKokkos = false;

// Initialize Kokkos, if it needs initialization.
// This takes the same arguments as (the first two of) initialize().
void initKokkos( int *argc, char ***argv )
{
    if ( !dtkInitializedKokkos )
    {
        // Kokkos doesn't have a global is_initialized().  However,
        // Kokkos::initialize() always initializes the default execution
        // space, so it suffices to check whether that was initialized.
        const bool kokkosIsInitialized =
            Kokkos::DefaultExecutionSpace::is_initialized();

        if ( !kokkosIsInitialized )
        {
            // Unlike MPI_Init, Kokkos promises not to modify argc and argv.
            Kokkos::initialize( *argc, *argv );
            dtkInitializedKokkos = true;
        }
    }

    const bool kokkosIsInitialized =
        Kokkos::DefaultExecutionSpace::is_initialized();

    if ( !kokkosIsInitialized )
        throw DataTransferKitException( "At the end of initKokkos, Kokkos"
                                        " is not initialized. Please report"
                                        " this bug to the DTK developers." );
}
} // namespace (anonymous)

void initialize( int *argc, char ***argv )
{
    if ( !dtkIsInitialized )
        initKokkos( argc, argv );
    dtkIsInitialized = true;
}

bool isInitialized() { return dtkIsInitialized; }

void finalize()
{
    if ( !dtkIsInitialized )
        return;

    // DTK should only finalize Kokkos if it initialized it
    if ( dtkInitializedKokkos )
        Kokkos::finalize();

    dtkIsInitialized = false;
}
}
