/****************************************************************************
 * Copyright (c) 2012-2018 by the DataTransferKit authors                   *
 * All rights reserved.                                                     *
 *                                                                          *
 * This file is part of the DataTransferKit library. DataTransferKit is     *
 * distributed under a BSD 3-clause license. For the licensing terms see    *
 * the LICENSE file in the top-level directory.                             *
 *                                                                          *
 * SPDX-License-Identifier: BSD-3-Clause                                    *
 ****************************************************************************/

#include <DTK_LinearBVH.hpp>

#include <Kokkos_DefaultNode.hpp>
#include <Teuchos_CommandLineProcessor.hpp>
#include <Teuchos_StandardCatchMacros.hpp>

#include <chrono>
#include <cmath> // cbrt
#include <random>

template <class NO>
int main_( Teuchos::CommandLineProcessor &clp, int argc, char *argv[] )
{
    using DeviceType = typename NO::device_type;
    using ExecutionSpace = typename DeviceType::execution_space;

    int n_values = 50000;
    int n_queries = 20000;

    clp.setOption( "values", &n_values, "number of indexable values (source)" );
    clp.setOption( "queries", &n_queries, "number of queries (target)" );

    clp.recogniseAllOptions( true );
    switch ( clp.parse( argc, argv ) )
    {
    case Teuchos::CommandLineProcessor::PARSE_HELP_PRINTED:
        return EXIT_SUCCESS;
    case Teuchos::CommandLineProcessor::PARSE_ERROR:
    case Teuchos::CommandLineProcessor::PARSE_UNRECOGNIZED_OPTION:
        return EXIT_FAILURE;
    case Teuchos::CommandLineProcessor::PARSE_SUCCESSFUL:
        break;
    }

    Kokkos::View<DataTransferKit::Point *, DeviceType> random_points(
        "random_points" );
    {
        auto n = std::max( n_values, n_queries );
        Kokkos::resize( random_points, n );

        auto random_points_host = Kokkos::create_mirror_view( random_points );

        // edge length chosen such that object density will remain constant as
        // problem size is changed
        auto const a = std::cbrt( n_values );
        std::uniform_real_distribution<double> distribution( -a, +a );
        std::default_random_engine generator;
        auto random = [&distribution, &generator]() {
            return distribution( generator );
        };
        for ( int i = 0; i < n; ++i )
            random_points_host( i ) = {{random(), random(), random()}};
        Kokkos::deep_copy( random_points, random_points_host );
    }

    Kokkos::View<DataTransferKit::Box *, DeviceType> bounding_boxes(
        "bounding_boxes", n_values );
    Kokkos::parallel_for( Kokkos::RangePolicy<ExecutionSpace>( 0, n_values ),
                          KOKKOS_LAMBDA( int i ) {
                              double const x = random_points( i )[0];
                              double const y = random_points( i )[1];
                              double const z = random_points( i )[2];
                              bounding_boxes( i ) = {
                                  {{x - 1., y - 1., z - 1.}},
                                  {{x + 1., y + 1., z + 1.}}};
                          } );
    Kokkos::fence();

    std::ostream &os = std::cout;

    auto start = std::chrono::high_resolution_clock::now();
    DataTransferKit::BVH<DeviceType> bvh( bounding_boxes );
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed_seconds = end - start;
    os << "construction " << elapsed_seconds.count() << "\n";

    {
        Kokkos::View<
            DataTransferKit::Details::Nearest<DataTransferKit::Point> *,
            DeviceType>
            queries( "queries", n_queries );
        Kokkos::parallel_for(
            Kokkos::RangePolicy<ExecutionSpace>( 0, n_queries ),
            KOKKOS_LAMBDA( int i ) {
                queries( i ) =
                    DataTransferKit::Details::nearest<DataTransferKit::Point>(
                        random_points( i ), 10 );
            } );
        Kokkos::fence();

        Kokkos::View<int *, DeviceType> offset( "offset" );
        Kokkos::View<int *, DeviceType> indices( "indices" );
        start = std::chrono::high_resolution_clock::now();
        bvh.query( queries, indices, offset );
        end = std::chrono::high_resolution_clock::now();
        elapsed_seconds = end - start;
        os << "knn " << elapsed_seconds.count() << "\n";
    }

    {
        Kokkos::View<DataTransferKit::Details::Within *, DeviceType> queries(
            "queries", n_queries );
        // radius chosen such that there will be approximately 10 results per
        // query
        double const pi = 3.14159265359;
        double const r = std::cbrt( 10. * 3. / ( 4. * pi ) );
        Kokkos::parallel_for(
            Kokkos::RangePolicy<ExecutionSpace>( 0, n_queries ),
            KOKKOS_LAMBDA( int i ) {
                queries( i ) =
                    DataTransferKit::Details::within( random_points( i ), r );
            } );
        Kokkos::fence();

        Kokkos::View<int *, DeviceType> offset( "offset" );
        Kokkos::View<int *, DeviceType> indices( "indices" );
        start = std::chrono::high_resolution_clock::now();
        bvh.query( queries, indices, offset );
        end = std::chrono::high_resolution_clock::now();
        elapsed_seconds = end - start;
        os << "radius " << elapsed_seconds.count() << "\n";
    }

    return 0;
}

int main( int argc, char *argv[] )
{
    Kokkos::initialize( argc, argv );

    bool success = true;
    bool verbose = true;

    try
    {
        const bool throwExceptions = false;

        Teuchos::CommandLineProcessor clp( throwExceptions );

        std::string node = "";
        clp.setOption( "node", &node, "node type (serial | openmp | cuda)" );

        clp.recogniseAllOptions( false );
        switch ( clp.parse( argc, argv, NULL ) )
        {
        case Teuchos::CommandLineProcessor::PARSE_ERROR:
            success = false;
        case Teuchos::CommandLineProcessor::PARSE_HELP_PRINTED:
        case Teuchos::CommandLineProcessor::PARSE_UNRECOGNIZED_OPTION:
        case Teuchos::CommandLineProcessor::PARSE_SUCCESSFUL:
            break;
        }

        if ( !success )
        {
            // do nothing, just skip other if clauses
        }
        else if ( node == "" )
        {
            typedef KokkosClassic::DefaultNode::DefaultNodeType Node;
            main_<Node>( clp, argc, argv );
        }
        else if ( node == "serial" )
        {
#ifdef KOKKOS_HAVE_SERIAL
            typedef Kokkos::Compat::KokkosSerialWrapperNode Node;
            main_<Node>( clp, argc, argv );
#else
            throw std::runtime_error( "Serial node type is disabled" );
#endif
        }
        else if ( node == "openmp" )
        {
#ifdef KOKKOS_HAVE_OPENMP
            typedef Kokkos::Compat::KokkosOpenMPWrapperNode Node;
            main_<Node>( clp, argc, argv );
#else
            throw std::runtime_error( "OpenMP node type is disabled" );
#endif
        }
        else if ( node == "cuda" )
        {
#ifdef KOKKOS_HAVE_CUDA
            typedef Kokkos::Compat::KokkosCudaWrapperNode Node;
            main_<Node>( clp, argc, argv );
#else
            throw std::runtime_error( "CUDA node type is disabled" );
#endif
        }
        else
        {
            throw std::runtime_error( "Unrecognized node type" );
        }
    }
    TEUCHOS_STANDARD_CATCH_STATEMENTS( verbose, std::cerr, success );

    Kokkos::finalize();

    return ( success ? EXIT_SUCCESS : EXIT_FAILURE );
}
