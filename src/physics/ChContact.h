#ifndef CHCONTACT_H
#define CHCONTACT_H

///////////////////////////////////////////////////
//
//   ChContact.h
//
//   Classes for enforcing constraints (contacts)
//   created by collision detection.
//
//   HEADER file for CHRONO,
//	 Multibody dynamics engine
//
// ------------------------------------------------
// 	 Copyright:Alessandro Tasora / DeltaKnowledge
//             www.deltaknowledge.com
// ------------------------------------------------
///////////////////////////////////////////////////


#include "core/ChFrame.h"
#include "lcp/ChLcpConstraintTwoContactN.h"
#include "lcp/ChLcpSystemDescriptor.h"
#include "collision/ChCCollisionModel.h"

namespace chrono
{


///
/// Class representing an unilateral contact constraint
/// between two 6DOF rigid bodies.
///

class ChContact {

protected:
				//
	  			// DATA
				//
	collision::ChCollisionModel* modA;	///< model A
	collision::ChCollisionModel* modB;  ///< model B

	ChVector<> p1;			///< max penetration point on geo1, after refining, in abs space
	ChVector<> p2;			///< max penetration point on geo2, after refining, in abs space
	ChVector<float> normal;	///< normal, on surface of master reference (geo1)

							///< the plane of contact (X is normal direction)
	ChMatrix33<float> contact_plane;
	
	double norm_dist;	    ///< penetration distance (negative if going inside) after refining

	float* reactions_cache; ///< points to an array[3] of N,U,V reactions which might be stored in a persistent contact manifold in the collision engine

							// The three scalar constraints, to be feed into the 
							// system solver. They contain jacobians data and special functions.
	ChLcpConstraintTwoContactN  Nx;
	ChLcpConstraintTwoFrictionT Tu;
	ChLcpConstraintTwoFrictionT Tv; 

	ChVector<> react_force;

public:
				//
	  			// CONSTRUCTORS
				//

	ChContact ();

	ChContact (			collision::ChCollisionModel* mmodA,	///< model A
						collision::ChCollisionModel* mmodB,	///< model B
						const ChLcpVariablesBody* varA, ///< pass A vars
						const ChLcpVariablesBody* varB, ///< pass B vars
						const ChFrame<>* frameA,		///< pass A frame
						const ChFrame<>* frameB,		///< pass B frame
						const ChVector<>& vpA,			///< pass coll.point on A, in absolute coordinates
						const ChVector<>& vpB,			///< pass coll.point on B, in absolute coordinates
						const ChVector<>& vN, 			///< pass coll.normal, respect to A, in absolute coordinates
						double mdistance,				///< pass the distance (negative for penetration)
						float* mreaction_cache,			///< pass the pointer to array of N,U,V reactions: a cache in contact manifold. If not available=0.
						float  mfriction				///< friction coeff.
				);

	virtual ~ChContact ();


				//
	  			// FUNCTIONS
				//

					/// Initialize again this constraint.
	virtual void Reset(	collision::ChCollisionModel* mmodA,	///< model A
						collision::ChCollisionModel* mmodB,	///< model B
						const ChLcpVariablesBody* varA, ///< pass A vars
						const ChLcpVariablesBody* varB, ///< pass B vars
						const ChFrame<>* frameA,		///< pass A frame
						const ChFrame<>* frameB,		///< pass B frame
						const ChVector<>& vpA,			///< pass coll.point on A, in absolute coordinates
						const ChVector<>& vpB,			///< pass coll.point on B, in absolute coordinates
						const ChVector<>& vN, 			///< pass coll.normal, respect to A, in absolute coordinates
						double mdistance,				///< pass the distance (negative for penetration)
						float* mreaction_cache,			///< pass the pointer to array of N,U,V reactions: a cache in contact manifold. If not available=0.
						float mfriction					///< friction coeff.
				);

					/// Get the contact coordinate system, expressed in absolute frame.
					/// This represents the 'main' reference of the link: reaction forces 
					/// are expressed in this coordinate system. Its origin is point P2.
					/// (It is the coordinate system of the contact plane and normal)
	virtual ChCoordsys<> GetContactCoords();

					/// Returns the pointer to a contained 3x3 matrix representing the UV and normal
					/// directions of the contact. In detail, the X versor (the 1s column of the 
					/// matrix) represents the direction of the contact normal.
	ChMatrix33<float>* GetContactPlane() {return &contact_plane;};


					/// Get the contact point 1, in absolute coordinates
	virtual ChVector<> GetContactP1() {return p1; };

					/// Get the contact point 2, in absolute coordinates
	virtual ChVector<> GetContactP2() {return p2; };

					/// Get the contact normal, in absolute coordinates
	virtual ChVector<float> GetContactNormal()  {return normal; };

					/// Get the contact distance
	virtual double	   GetContactDistance()  {return norm_dist; };
	
					/// Get the contact force, if computed, in contact coordinate system
	virtual ChVector<> GetContactForce() {return react_force; };

					/// Get the contact friction coefficient
	virtual float GetFriction() {return Nx.GetFrictionCoefficient(); };
					/// Set the contact friction coefficient
	virtual void SetFriction(float mf) { Nx.SetFrictionCoefficient(mf); };

					/// Get the collision model A, with point P1
	virtual collision::ChCollisionModel* GetModelA() {return this->modA;}
					/// Get the collision model B, with point P2
	virtual collision::ChCollisionModel* GetModelB() {return this->modB;}

				//
				// UPDATING FUNCTIONS
				//

	
	void  InjectConstraints(ChLcpSystemDescriptor& mdescriptor);

	void  ConstraintsBiReset();

	void  ConstraintsBiLoad_C(double factor=1., double recovery_clamp=0.1, bool do_clamp=false);

	void  ConstraintsFetch_react(double factor);

	void  ConstraintsLiLoadSuggestedSpeedSolution();

	void  ConstraintsLiLoadSuggestedPositionSolution();

	void  ConstraintsLiFetchSuggestedSpeedSolution();

	void  ConstraintsLiFetchSuggestedPositionSolution();

};




//////////////////////////////////////////////////////
//////////////////////////////////////////////////////


} // END_OF_NAMESPACE____

#endif
