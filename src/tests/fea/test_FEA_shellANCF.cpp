//
// PROJECT CHRONO - http://projectchrono.org
//
// Copyright (c) 2013 Project Chrono
// All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file at the top level of the distribution
// and at http://projectchrono.org/license-chrono.txt.
//

#include <algorithm>
#include <string>

#include "chrono/ChConfig.h"
#include "chrono/core/ChFileutils.h"
#include "chrono/core/ChTimer.h"
#include "chrono/solver/ChSolverMINRES.h"
#include "chrono/physics/ChBodyEasy.h"
#include "chrono/physics/ChSystem.h"
#include "chrono/utils/ChUtilsInputOutput.h"

#include "chrono_fea/ChElementShellANCF.h"
#include "chrono_fea/ChLinkDirFrame.h"
#include "chrono_fea/ChLinkPointFrame.h"
#include "chrono_fea/ChMesh.h"

#ifdef CHRONO_MKL
#include "chrono_mkl/ChSolverMKL.h"
#endif

#ifdef CHRONO_OPENMP_ENABLED
#include <omp.h>
#endif

using namespace chrono;
using namespace chrono::fea;

// -----------------------------------------------------------------------------

int num_threads = 4;      // default number of threads
double step_size = 1e-3;  // integration step size
int num_steps = 20;       // number of integration steps
int skip_steps = 2;       // initial number of steps excluded from timing

int numDiv_x = 50;  // mesh divisions in X direction
int numDiv_y = 50;  // mesh divisions in Y direction
int numDiv_z = 1;   // mesh divisions in Z direction

std::string out_dir = "../TEST_SHELL_ANCF";  // name of output directory
bool output = false;                         // generate output file?
bool verbose = false;                        // verbose output?

// -----------------------------------------------------------------------------

void RunModel(bool use_mkl,              // use MKL solver (if available)
              bool use_adaptiveStep,     // allow step size reduction
              bool use_modifiedNewton,   // use modified Newton method
              const std::string& suffix  // output filename suffix
              ) {
#ifndef CHRONO_MKL
    use_mkl = false;
#endif

    std::cout << std::endl;
    std::cout << "===================================================================" << std::endl;
    std::cout << "Solver:          " << (use_mkl ? "MKL" : "MINRES") << std::endl;
    std::cout << "Adaptive step:   " << (use_adaptiveStep ? "Yes" : "No") << std::endl;
    std::cout << "Modified Newton: " << (use_modifiedNewton ? "Yes" : "No") << std::endl;
    std::cout << std::endl;
    std::cout << "Mesh divisions:  " << numDiv_x << " x " << numDiv_y << std::endl;
    std::cout << std::endl;

    // Create the physical system
    ChSystem my_system;

    my_system.Set_G_acc(ChVector<>(0, 0, -9.81));

    // Create a mesh, that is a container for groups of elements and their referenced nodes.
    auto my_mesh = std::make_shared<ChMesh>();

    // Geometry of the plate
    double plate_lenght_x = 1.0;
    double plate_lenght_y = 1.0;
    double plate_lenght_z = 0.04;  // small thickness
    // Specification of the mesh
    int N_x = numDiv_x + 1;
    int N_y = numDiv_y + 1;
    int N_z = numDiv_z + 1;
    // Number of elements in the z direction is considered as 1
    int TotalNumElements = numDiv_x * numDiv_y;
    //(1+1) is the number of nodes in the z direction
    int TotalNumNodes = (numDiv_x + 1) * (numDiv_y + 1);  // Or *(numDiv_z+1) for multilayer
    // Element dimensions (uniform grid)
    double dx = plate_lenght_x / numDiv_x;
    double dy = plate_lenght_y / numDiv_y;
    double dz = plate_lenght_z / numDiv_z;

    // Create and add the nodes
    for (int i = 0; i < TotalNumNodes; i++) {
        // Parametric location and direction of nodal coordinates
        double loc_x = (i % (numDiv_x + 1)) * dx;
        double loc_y = (i / (numDiv_x + 1)) % (numDiv_y + 1) * dy;
        double loc_z = (i) / ((numDiv_x + 1) * (numDiv_y + 1)) * dz;

        double dir_x = 0;
        double dir_y = 0;
        double dir_z = 1;

        // Create the node
        auto node = std::make_shared<ChNodeFEAxyzD>(ChVector<>(loc_x, loc_y, loc_z), ChVector<>(dir_x, dir_y, dir_z));
        node->SetMass(0);
        // Fix all nodes along the axis X=0
        if (i % (numDiv_x + 1) == 0)
            node->SetFixed(true);

        // Add node to mesh
        my_mesh->AddNode(node);
    }

    // Create an isotropic material
    // Only one layer
    double rho = 500;
    double E = 2.1e7;
    double nu = 0.3;
    auto mat = std::make_shared<ChMaterialShellANCF>(rho, E, nu);

    // Create the elements
    for (int i = 0; i < TotalNumElements; i++) {
        // Definition of nodes forming an element
        int node0 = (i / (numDiv_x)) * (N_x) + i % numDiv_x;
        int node1 = (i / (numDiv_x)) * (N_x) + i % numDiv_x + 1;
        int node2 = (i / (numDiv_x)) * (N_x) + i % numDiv_x + 1 + N_x;
        int node3 = (i / (numDiv_x)) * (N_x) + i % numDiv_x + N_x;

        // Create the element and set its nodes.
        auto element = std::make_shared<ChElementShellANCF>();
        element->SetNodes(std::dynamic_pointer_cast<ChNodeFEAxyzD>(my_mesh->GetNode(node0)),
                          std::dynamic_pointer_cast<ChNodeFEAxyzD>(my_mesh->GetNode(node1)),
                          std::dynamic_pointer_cast<ChNodeFEAxyzD>(my_mesh->GetNode(node2)),
                          std::dynamic_pointer_cast<ChNodeFEAxyzD>(my_mesh->GetNode(node3)));

        // Element length is a fixed number in both direction. (uniform distribution of nodes in both directions)
        element->SetDimensions(dx, dy);
        // Single layer
        element->AddLayer(dz, 0 * CH_C_DEG_TO_RAD, mat);  // Thickness: dy;  Ply angle: 0.
        // Set other element properties
        element->SetAlphaDamp(0.0);   // Structural damping for this
        element->SetGravityOn(true);  // element calculates its own gravitational load
        // Add element to mesh
        my_mesh->AddElement(element);
    }

    // Switch off mesh class gravity (ANCF shell elements have a custom implementation)
    my_mesh->SetAutomaticGravity(false);

    // Remember to add the mesh to the system!
    my_system.Add(my_mesh);

    // Mark completion of system construction
    my_system.SetupInitial();

    // Set up solver
    if (use_mkl) {
#ifdef CHRONO_MKL
        ChSolverMKL* mkl_solver_stab = new ChSolverMKL;
        ChSolverMKL* mkl_solver_speed = new ChSolverMKL;
        my_system.ChangeSolverStab(mkl_solver_stab);
        my_system.ChangeSolverSpeed(mkl_solver_speed);
        mkl_solver_speed->SetSparsityPatternLock(true);
        mkl_solver_stab->SetSparsityPatternLock(true);
#endif
    } else {
        my_system.SetSolverType(ChSystem::SOLVER_MINRES);
        ChSolverMINRES* msolver = (ChSolverMINRES*)my_system.GetSolverSpeed();
        msolver->SetDiagonalPreconditioning(true);
        my_system.SetMaxItersSolverSpeed(100);
        my_system.SetTolForce(1e-10);
    }

    // Set up integrator
    my_system.SetIntegrationType(ChSystem::INT_HHT);
    auto mystepper = std::static_pointer_cast<ChTimestepperHHT>(my_system.GetTimestepper());
    mystepper->SetAlpha(-0.2);
    mystepper->SetMaxiters(100);
    mystepper->SetAbsTolerances(1e-5);
    mystepper->SetMode(ChTimestepperHHT::POSITION);
    mystepper->SetStepControl(use_adaptiveStep);
    mystepper->SetModifiedNewton(use_modifiedNewton);
    mystepper->SetScaling(true);
    mystepper->SetVerbose(verbose);

    // Initialize the output stream and set precision.
    utils::CSV_writer out("\t");
    out.stream().setf(std::ios::scientific | std::ios::showpos);
    out.stream().precision(6);

    // Get handle to tracked node.
    auto nodetip = std::dynamic_pointer_cast<ChNodeFEAxyzD>(my_mesh->GetNode(TotalNumNodes - 1));

    // Simulation loop
    ChTimer<> timer;
    double time_total = 0;
    int num_iterations = 0;
    int num_setup_calls = 0;
    int num_solver_calls = 0;

    for (int istep = 0; istep < num_steps; istep++) {
        timer.reset();
        timer.start();
        my_system.DoStepDynamics(step_size);
        timer.stop();

        const ChVector<>& p = nodetip->GetPos();
        double time_step = timer.GetTimeSeconds();

        if (istep <= skip_steps) {
            time_total = 0;
            my_mesh->ResetTimers();
        }

        time_total += time_step;
        num_iterations += mystepper->GetNumIterations();
        num_setup_calls += mystepper->GetNumSetupCalls();
        num_solver_calls += mystepper->GetNumSolveCalls();

        if (verbose) {
            std::cout << "-------------------------------------------------------------------" << std::endl;
            std::cout << my_system.GetChTime() << "  ";
            std::cout << "   " << time_step;
            std::cout << "   [ " << p.x << " " << p.y << " " << p.z << " ]" << std::endl;
        }

        if (output) {
            out << my_system.GetChTime() << time_step << nodetip->GetPos() << std::endl;
        }
    }

    // Final statistics.
    double time_force = my_mesh->GetTimingInternalForces();
    double time_jac = my_mesh->GetTimingJacobianLoad();

    std::cout << "-------------------------------------------------------------------" << std::endl;
    std::cout << "Total number of steps:        " << num_steps << std::endl;
    std::cout << "Total number of iterations:   " << num_iterations << std::endl;
    std::cout << "Total number of setup calls:  " << num_setup_calls << std::endl;
    std::cout << "Total number of solver calls: " << num_solver_calls << std::endl;
    std::cout << "Total number of internal force calls: " << my_mesh->GetNumCallsInternalForces() << std::endl;
    std::cout << "Total number of Jacobian calls:       " << my_mesh->GetNumCallsJacobianLoad() << std::endl;
    std::cout << "Simulation times (cummulative over the last " << num_steps - skip_steps << " steps)" << std::endl;
    std::cout << "  Total execution: " << time_total << std::endl;
    std::cout << "  Internal forces: " << time_force << std::endl;
    std::cout << "  Jacobian:        " << time_jac << std::endl;
    std::cout << "  Extra time:      " << time_total - time_force - time_jac << std::endl;

    if (output) {
        char name[100];
        std::sprintf(name, "%s/out_%s_%d.txt", out_dir.c_str(), suffix.c_str(), num_threads);
        std::cout << "Write output to: " << name << std::endl;
        out.write_to_file(name);
    }
}

int main(int argc, char* argv[]) {
    // Create output directory (if it does not already exist).
    if (output) {
        if (ChFileutils::MakeDirectory("../TEST_SHELL_ANCF") < 0) {
            GetLog() << "Error creating directory ../TEST_SHELL_ANCF\n";
            return 1;
        }
    }

#ifdef CHRONO_OPENMP_ENABLED
    // Set number of threads
    if (argc > 1)
        num_threads = std::stoi(argv[1]);
    num_threads = std::min(num_threads, CHOMPfunctions::GetNumProcs());
    CHOMPfunctions::SetNumThreads(num_threads);
    GetLog() << "Using " << num_threads << " thread(s)\n";
#else
    GetLog() << "No OpenMP\n";
#endif

    // Run simulations.
    RunModel(true, true, false, "MKL_adaptive_full");     // MKL, adaptive step, full Newton
    RunModel(true, true, true, "MKL_adaptive_modified");  // MKL, adaptive step, modified Newton

    RunModel(false, true, false, "MINRES_adaptive_full");     // MINRES, adaptive step, full Newton
    RunModel(false, true, true, "MINRES_adaptive_modified");  // MINRES, adaptive step, modified Newton

    return 0;
}
