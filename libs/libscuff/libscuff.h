/* Copyright (C) 2005-2011 M. T. Homer Reid
 *
 * This file is part of SCUFF-EM.
 *
 * SCUFF-EM is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * SCUFF-EM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * libscuff.h    -- header file for libscuff
 *
 * this file is long, but basically is divided into several digestible
 * sections:
 *
 *  1. class definitions for RWGSurface and supporting classes
 *  2. class definition for RWGGeometry
 *  3. non-class methods that operate on RWGPanels or RWGSurfaces
 *  4. other lower-level non-class methods 
 *
 * homer reid  -- 3/2007 -- 9/2011
 */

#ifndef LIBSCUFF_H
#define LIBSCUFF_H

#include <stdio.h>
#include <math.h>
#include <stdarg.h>
#include <complex>
#include <cmath>
#include <vector>

#include <libhrutil.h>
#include <libhmat.h>
#include <libMatProp.h>
#include <libIncField.h>
#include <libSubstrate.h>

#include "GTransformation.h"
#include "GBarAccelerator.h"
#include "PFTOptions.h"
#include "EquivalentEdgePairs.h"

namespace scuff {

/*--------------------------------------------------------------*/
/*- some constants used to pass values to libscuff functions   -*/
/*--------------------------------------------------------------*/
#define SCUFF_GENERALFREQ  0
#define SCUFF_PUREIMAGFREQ 1

#define SCUFF_NOLOGGING      0
#define SCUFF_TERSELOGGING   1
#define SCUFF_VERBOSELOGGING 2
#define SCUFF_VERBOSE2       3

// maximum number of lattice basis vectors
#ifndef MAXLDIM
#define MAXLDIM 3
#endif

/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/
/* impedance of free space */
#ifndef ZVAC
#define ZVAC 376.73031346177
#endif


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/
/*- 1. Class definitions for RWGSurface and supporting classes -*/
/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/ 

/***************************************************************/
/* RWGPanel is a structure containing data on a single         */
/* triangular panel in the meshed geometry.                    */
/*                                                             */
/* note: after the following code snippet                      */
/*  double *R= O->Vertices + 3*O->Panels[np]->VI[i]            */
/* we have that R[0..2] are the cartesian coordinates of the   */
/* ith vertex (i=0,1,2) of the npth panel in surface O.        */
/*                                                             */
/***************************************************************/
typedef struct RWGPanel
 { 
   int VI[3];                /* indices of vertices in Vertices array */
   int EI[3];                /* indices of edges in Edges array */
   double Centroid[3];       /* panel centroid */
   double ZHat[3];           /* normal vector */
   bool ZHatFlipped;         /* true / false ==> ZHat obeys right (left)-hand rule wrt VI*/
   double Radius;            /* radius of enclosing sphere */
   double Area;              /* panel area */
   int Index;                /* index of this panel within RWGSurface (0..NumPanelsP-1)*/

} RWGPanel;

/***************************************************************/
/* RWGEdge is a structure containing data on a single edge     */
/* in a meshed surfaces, corresponding to a single RWG basis   */
/* function.                                                   */
/*                                                             */
/* This may be a *full* RWG function, supported on two panels  */
/* (corresponding to an *interior* edge in the discretization),*/
/* or it may be a *half* RWG (HRWG) function, supported on     */
/* only one panel. HRWG functions are created for all exterior */
/* edges, and in some cases may be created for interior edges  */
/* well (to define point-like RWGPorts in RF devices.)         */
/* RWGEdges for HRWG functions have iQM=iMPanel=MIndex=-1.     */
/*                                                             */
/* note: after the following code snippet                      */
/*  double *QP = S->Vertices + 3*E->IQP                        */
/* we have that QP[0..2] are the cartesian coordinates of the  */
/* QP vertex in the basis function corresponding to edge E.    */
/***************************************************************/
typedef struct RWGEdge 
 { 
   int iV1, iV2, iQP, iQM;	/* indices of panel vertices (iV1<iV2) */
   double Centroid[3];          /* edge centroid */
   double Length;               /* length of edge */
   double Radius;               /* radius of enclosing sphere */

   int iPPanel;                 /* index of PPanel within RWGSurface (0..NumPanelsP-1)*/
   int iMPanel;                 /* index of MPanel within RWGSurface (0..NumPanelsP-1)*/
   int PIndex;                  /* index of this edge within PPanel (0..2)*/
   int MIndex;                  /* index of this edge within MPanel (0..2)*/
   int Index;                   /* index of this edge within RWGSurface (0..NumEdges-1)*/

   RWGEdge *Next;               /* pointer to next edge in linked list */

} RWGEdge;

/***************************************************************/
/* fast kd-tree based point-in-object calculations             */
/***************************************************************/
typedef struct kdtri_s *kdtri;
void kdtri_destroy(kdtri t); // destructor

// some tree statistics for informational purposes:
unsigned kdtri_maxdepth(kdtri t);
size_t kdtri_maxleaf(kdtri t);
double kdtri_meandepth(kdtri t);
double kdtri_meanleaf(kdtri t);

/***************************************************************/
/* prototype for caller-supplied routine used to generate      */
/* visualization plots of surface quantities.                  */
/* MDFunc inputs a list of NX coordinates (in the form of the  */
/* NX x 3 matrix XMatrix) and returns a new NX x ND HMatrix    */
/* whose columns are the ND data quantities.                   */
/***************************************************************/
typedef HMatrix *(*MeshDataFunc)(void *UserData, HMatrix *XMatrix, const char ***DataNames);

/***************************************************************/
/* RWGSurface is a class describing a single contiguous surface*/
/* lying at the interface between two regions. The surface may */
/* be closed or open.                                          */
/***************************************************************/
class RWGSurface
 { 
   /*--------------------------------------------------------------*/ 
   /*- class methods ----------------------------------------------*/ 
   /*--------------------------------------------------------------*/ 
  public:  

   /*-------------------------------------------------------------------*/
   /* main constructor entry point: construct from 'OBJECT...ENDOBJECT' */
   /* or SURFACE...ENDSURFACE section in a .scuffgeo file               */
   /*-------------------------------------------------------------------*/
   RWGSurface(FILE *f, const char *Label, int *LineNum, char *Keyword);

   /*-----------------------------------------------------------------*/
   /* alternative constructor entry points that define an RWGSurface  */
   /* which exists independently of any RWGGeometry structure. This   */
   /* means the surface doesn't know what regions it bounds or what   */
   /* materials are on its two sides, but can still be used for       */
   /* low-level computations at the level of individual panels/edges. */
   /*-----------------------------------------------------------------*/
   RWGSurface(const char *MeshFile, int MeshTag=-1);
   RWGSurface(dVec VertexCoordinates, iVec PanelVertexIndices, char *pLabel=0);
   RWGSurface(dVec X0, dVec L1, dVec L2, int N1, int N2=0, char *pLabel=0);

   /* destructor */
   ~RWGSurface();

   /* get overlap integrals between two basis functions */
   double GetOverlap(int neAlpha, int neBeta, double *pOTimes = NULL);
   void GetOverlaps(int neAlpha, int neBeta, double *Overlaps);

   /* apply a general transformation (rotation+displacement) to the surface */
   void Transform(const GTransformation *GT);
   void Transform(const char *format, ...);
   void UnTransform();

   /* fast inclusion tests */
   bool Contains(const double X[3]);
   bool Contains(const RWGSurface *S);

   /* visualization */
   // void Visualize(double *KVec, double Kappa, char *format, ...);
   void WriteGPMesh(const char *format, ...);
   void WritePPMesh(const char *FileName, const char *Tag, bool PlotNormals=false);
   void WritePPMeshLabels(const char *FileName, const char *Tag, int WhichLabels);
   void WritePPMeshLabels(const char *FileName, const char *Tag);
   double PlotScalarDensity(int PlotType, void *Values, int AveragingMethod,
                            const char *FileName, const char *ViewNameFormat, ...);
   double PlotScalarDensity(double *Values, int AveragingMethod, const char *FileName,
                            const char *ViewNameFormat, ...);
   double PlotScalarDensity(int PlotType, cdouble *Values, int AveragingMethod, 
                            const char *FileName, const char *ViewNameFormat, ...);
   double PlotScalarDensity(int PlotType, HMatrix *M, int WhichColumn,
                            int AveragingMethod, const char *FileName, const char *ViewNameFormat, ...);
   double PlotVectorDensity(int PlotType, void *Values[3], const char *FileName,
                            const char *ViewNameFormat, ...);
   double PlotVectorDensity(double *Values[3], const char *FileName,
                            const char *ViewNameFormat, ...);
   double PlotVectorDensity(int PlotType, HMatrix *M, int WhichColumn,
                            const char *FileName, const char *ViewNameFormat, ...);
   HVector *MakeMeshPlot(MeshDataFunc MDFunc, void *UserData=0,
                         const char *OutFileBase=0, const char *PPOptions=0,
                         bool UseCentroids=true);

//  private:

   /*--------------------------------------------------------------*/
   /*- private data fields  ---------------------------------------*/
   /*--------------------------------------------------------------*/
   char *RegionLabels[2];          /* names of the regions on either side of the surface */
   int RegionIndices[2];           /* indices of the two regions within the RWGGeometry list of Regions */
   bool IsPEC;                     /* =1 if this is a simple PEC object */
   bool IsObject;                  /* =1 if we came from an OBJECT...ENDOBJECT section */
                                   /* =0 if we came from a SURFACE...ENDSURFACE section */

   double *Vertices;               /* Vertices[3*n,3*n+1,3*n+2]=nth vertex coords */
   RWGPanel **Panels;              /* array of pointers to panels         */
   RWGEdge **Edges;                /* array of pointers to interior edges */
   RWGEdge **HalfRWGEdges;         /* array of pointers to exterior edges plus interior edges with half-RWG functions*/
   int IsClosed;                   /* = 1 for a closed surface, 0 for an open surface */
   double RMax[3], RMin[3];        /* bounding box corners */

   double tolVecClose;             /* absolute tolerance for VecClose */

   int NumVertices;                /* number of vertices in mesh  */
   int NumInteriorVertices;        /* number of interior vertices */
   int NumRefPts;                  /* number of vertices used as reference points */

   int NumEdges;                   /* number of interior edges     */
   int NumExteriorEdges;           /* number of exterior edges     */
   int NumHalfRWGEdges;            /* =NumExteriorEdges unless point RF ports are present */
   int NumTotalEdges;              /* NumEdges + NumHalfRWGEdges   */

   int NumBFs;                     /* number of basis functions (electric + magnetic)*/
   int NumPanels;                  /* number of panels */
   int NumBCs;                     /* number of boundary countours */

   int Index;                      /* index of this surface in geometry  */

   int *WhichBC;                   /* WhichBC[nv] = index of boundary contour */
                                   /* on which vertex #nv lies (=0 if vertex  */
                                   /* #nv is an internal vertex)              */
   int *NumBCEdges;                /* NumBCEdges[2] is the number of edges in */
                                   /* boundary contour #2                     */
   RWGEdge ***BCEdges;             /* BCEdges[2][3] is a pointer to the 3rd   */
                                   /* edge in boundary contour #2             */

   int NumRedundantVertices;

   char *MeshFileName;             /* saved name of mesh file */
   char *MeshFileDir;              /* directory in which mesh file was found */
   int MeshTag;                    /* index of entity within mesh file; = -1 if not applicable */
   char *Label;                    /* unique label identifying surface */

   kdtri kdPanels; /* kd-tree of panels */
   void InitkdPanels(bool reinit = false, int LogLevel = SCUFF_NOLOGGING);

   /* OTGT is a 'one-time geometry transformation' that is applied  */
   /* once to the mesh upon the initial read in from the mesh file  */
   /* and is not subsequently undone.                               */
   /* GT encodes a subsequent transformation that may be applied    */
   /* to the surface temporarily with the intention of eventually   */
   /* undoing it.                                                   */
   /* Origin is the point (0,0,0) in the original mesh file,        */
   /* transformed by OTGT and or any subsequent transformations.    */
   GTransformation *OTGT, *GT;
   double Origin[3];

   /* SurfaceZeta, if non-NULL, points to a cevaluator for a      */
   /* user-specified function of frequency and position (w,x,y,z) */
   /* describing surface impedance in units of ZVAC               */
   void *SurfaceZeta;

   // the following fields are used to pass some data items up to the 
   // higher-level routine that calls the RWGSurface constructor
   char *ErrMsg;                   /* used to indicate to a calling routine that an error has occurred */
   int MaterialRegionsLineNum;     /* line of .scuffgeo file on which MATERIAL or REGIONS keyword appeared */
   char *MaterialName;             /* name of material in OBJECT...ENDOBJECT section */

   // 20140327 explain me
   int TotalStraddlers;
   int *PhasedBFCs; // 'phased basis-function contributions'

   /*--------------------------------------------------------------*/ 
   /*- private class methods --------------------------------------*/ 
   /*--------------------------------------------------------------*/ 
   RWGEdge *GetEdgeByIndex(int ne);

   /* constructor helper subroutines */
   char *InitRWGSurface(dVec VertexCoordinates, iVec PanelVertexIndices);
   char *ParseOBJECTSURFACESection(FILE *f, int *LineNum);
   char *InitEdgeList();
   void AddStraddlers(HMatrix *LBasis, int NumStraddlers[MAXLDIM]);
   void UpdateBoundingBox();
 };

/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/
/*- 3. Class definition for RWGGeometry                        -*/
/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/
typedef struct MMJData
 { int NumEdges;
   int *SurfaceIndices, *EdgeIndices;
 } MMJData;

class EquivalentEdgePairTable; // forward declaration

/*************************** ***********************************/
/* an RWGGeometry is a collection of regions with interfaces   */
/* described by RWGSurfaces.                                   */
/***************************************************************/
class RWGGeometry 
 { 
   /*--------------------------------------------------------------*/ 
   /*- public class methods ---------------------------------------*/ 
   /*--------------------------------------------------------------*/ 
  public:  

   /* constructor / destructor */
   RWGGeometry(const char *GeoFileName, int pLogLevel = SCUFF_NOLOGGING);
   ~RWGGeometry();

   /*--------------------------------------------------------------*/
   /*- routines for setting up the BEM system, i.e. allocating and */
   /*- assembling the BEM matrix and RHS vector                    */
   /*--------------------------------------------------------------*/
   HMatrix *AllocateBEMMatrix(bool PureImagFreq = false, bool Packed = false);
   HMatrix *AssembleBEMMatrix(cdouble Omega, double *kBloch, HMatrix *M = NULL);
   HMatrix *AssembleBEMMatrix(cdouble Omega, HMatrix *M = NULL);

   HVector *AllocateRHSVector(bool PureImagFreq = false );
   HVector *AssembleRHSVector(cdouble Omega, double *kBloch,
                              IncField *IF, HVector *RHS = NULL);
   HVector *AssembleRHSVector(cdouble Omega, IncField *IF, HVector *RHS = NULL);

   /*--------------------------------------------------------------*/
   /*- post-processing routines for computing fields               */
   /*--------------------------------------------------------------*/
   int UpdateIncFields(IncField *IF, cdouble Omega, double *kBloch=0);

   HMatrix *GetFields(IncField *IF, HVector *KN, cdouble Omega, 
                      double *kBloch, HMatrix *XMatrix, HMatrix *FMatrix=NULL);
   HMatrix *GetFields(IncField *IF, HVector *KN, cdouble Omega,
                      HMatrix *XMatrix, HMatrix *FMatrix=NULL);

   void GetFields(IncField *IF, HVector *KN, cdouble Omega,
                  double *kBloch, double *X, cdouble *EH);
   void GetFields(IncField *IF, HVector *KN, cdouble Omega,
                  double *X, cdouble *EH);

   /*--------------------------------------------------------------*/
   /*- post-processing routine for dyadic green's functions -------*/
   /*--------------------------------------------------------------*/
   HMatrix *GetDyadicGFs(cdouble Omega, double *kBloch,
                         HMatrix *XMatrix, HMatrix *M,
                         HMatrix *GMatrix=0, 
                         bool ScatteringOnly=false);

   // these next two are legacy interfaces which will be
   // removed in future versions
   void GetDyadicGFs(double XEval[3], double XSource[3],
                     cdouble Omega, double kBloch[2],
                     HMatrix *M, HVector *KN,
                     cdouble GEScat[3][3], cdouble GMScat[3][3],
                     cdouble GETot[3][3],  cdouble GMTot[3][3]);

   void GetDyadicGFs(double X[3], cdouble Omega, double *kBloch,
                     HMatrix *M, HVector *KN,
                     cdouble GEScat[3][3], cdouble GMScat[3][3]);

   /*--------------------------------------------------------------*/
   /* post-processing routines for power, force, and torque (PFT)  */
   /*--------------------------------------------------------------*/
   void GetPFT(int SurfaceIndex, HVector *KN, cdouble Omega,
               double PFT[NUMPFT], PFTOptions *Options=0);
   HMatrix *GetPFTMatrix(HVector *KN, cdouble Omega,
                         PFTOptions *Options=0, HMatrix *PFTMatrix=0);

   /*--------------------------------------------------------------*/
   /*- post-processing routines for various other quantities      -*/
   /*--------------------------------------------------------------*/
   /* charge and current densities at panel centroids */
   HMatrix *GetPanelSourceDensities(cdouble Omega, double *kBloch, HVector *KN, HMatrix *PSD=0);
   HMatrix *GetPanelSourceDensities(cdouble Omega, HVector *KN, HMatrix *PSD=0);

   /* scattering amplitudes */
   cdouble GetPlaneWaveAmplitude(cdouble Omega, double *kBloch, HVector *KNVector, PlaneWave *PW, cdouble *Terms=0);
   cdouble GetPlaneWaveAmplitude(cdouble Omega, HVector *KNVector, PlaneWave *PW, cdouble *Terms=0);
   cdouble GetPlaneWaveAmplitude(cdouble Omega, double *kBloch, HVector *KNVector,
                                 cdouble E0[3], double nHat[3], char *RegionLabel=0, cdouble *Terms=0);
   cdouble GetPlaneWaveAmplitude(cdouble Omega, HVector *KNVector,
                                 cdouble E0[3], double nHat[3], char *RegionLabel=0, cdouble *Terms=0);

   /* electric and magnetic dipole moments */
   HMatrix *GetDipoleMoments(cdouble Omega, HVector *KN, HMatrix *PM=0, HMatrix *KNResolved=0);

   /* expansion coefficients in the RWG expansion of an arbitrary */
   /* user-supplied surface-tangential vector field               */
   void ExpandCurrentDistribution(IncField *IF, HVector *KN, cdouble Omega=1.0, bool IsEHField=false);

   /* get surface currents and charges at a given point X (zero if X not on a meshed surface)  */
   /* iw{Sigma,Tau} = \nabla \cdot {K, N}  = i\omega * {electric, magnetic} surface charge     */
   void EvalSourceDistribution(const double X[3], HVector *KNVector, double *kBloch,
                               cdouble KN[6], cdouble iwSigmaTau[2],
                               int *WhichSurface=0, int *WhichPanel=0);
   void EvalSourceDistribution(const double X[3], HVector *KNVector,
                               cdouble KN[6], cdouble iwSigmaTau[2],
                               int *WhichSurface=0, int *WhichPanel=0);
  
   void EvalCurrentDistribution(const double X[3], HVector *KNVector, double *kBloch, cdouble KN[6]);
   void EvalCurrentDistribution(const double X[3], HVector *KNVector, cdouble KN[6]);

   /*--------------------------------------------------------------*/
   /*- visualization routines -------------------------------------*/
   /*--------------------------------------------------------------*/
   void WritePPMesh(const char *FileName, const char *Tag, bool PlotNormals=false);
   void WriteGPMesh(const char *format, ...);
   void WriteGPMeshPlus(const char *format, ...);
   void PlotSurfaceCurrents(HMatrix *PSDMatrix, cdouble Omega, double *kBloch, const char *FileNameFormat, ...);
   void PlotSurfaceCurrents(HMatrix *PSDMatrix, cdouble Omega, const char *FileNameFormat,  ...);
   void PlotSurfaceCurrents(HVector *KN, cdouble Omega, double *kBloch, const char *FileNameFormat, ...);
   void PlotSurfaceCurrents(HVector *KN, cdouble Omega, const char *FileNameFormat, ...);

   /*--------------------------------------------------------------*/
   /*- misc routines for modifying geometric parameters, etc. -----*/
   /*--------------------------------------------------------------*/

   /* routine for setting logging verbosity */
   void SetLogLevel(int LogLevel);
   void SetLogLevel(const char *Level);

   /* routines for changing material properties */
   void SetEps(cdouble Eps);
   void SetEps(const char *Label, cdouble Eps);
   void SetEpsMu(cdouble Eps, cdouble Mu);
   void SetEpsMu(const char *Label, cdouble Eps, cdouble Mu);

   /* some simple utility functions */
   int GetDimension();
   int GetRegionByLabel(const char *Label);
   RWGSurface *GetSurfaceByLabel(const char *Label, int *pns=NULL);
   int GetRegionIndex(const double X[3]); // index of region containing X
   int PointInRegion(int RegionIndex, const double X[3]); 

   /* geometrical transformations */
   void Transform(GTComplex *GTC);
   void UnTransform();
   char *CheckGTCList(GTCList GTCs);

   /*--------------------------------------------------------------------*/
   /*- class methods intended for internal use only, i.e. which          */
   /*- would be private if we cared about the public/private distinction */
   /*--------------------------------------------------------------------*/
//private: 
   // constructor helper functions
   void ProcessMEDIUMSection(FILE *f, char *FileName, int *LineNum);
   void ProcessLATTICESection(FILE *f, char *FileName, int *LineNum);
   void AddRegion(char *RegionLabel, char *MaterialName, int LineNum);
   void InitPBCData();
   void DetectMultiMaterialJunctions();

   // helper functions for AssembleBEMMatrix
   void UpdateCachedEpsMuValues(cdouble Omega);
   void AssembleBEMMatrixBlock(int nsa, int nsb, cdouble Omega, double *kBloch,
                               HMatrix *M, HMatrix **GradM=0,
                               int RowOffset=0, int ColOffset=0,
                               void *ABMBCache=0, bool CacheTranspose=false,
                               int NumTorqueAxes=0, HMatrix **dMdT=0,
                               double *GammaMatrix=0);
   void *CreateABMBAccelerator(int nsa, int nsb, bool PureImagFreq=false,
                               bool NeedZDerivative=false);
   void DestroyABMBAccelerator(void *Accelerator);
   void ApplyMMJTransformation(HMatrix *M, HVector *RHS);

   // helper function for GetFields, GetDyadicGFs, GetSRFluxTrace
   HMatrix *GetRFMatrix(cdouble Omega, double *kBloch, HMatrix *XMatrix,
                        HMatrix *RFMatrix=0, bool MinuskBloch=false,
                        int ColumnOffset=0);

   // helper function for accelerating periodic GF calculations
   GBarAccelerator *CreateRegionGBA(int nr, cdouble Omega, double *kBloch, int ns1, int ns2);
   GBarAccelerator *CreateRegionGBA(int nr, cdouble Omega, double *kBloch, HMatrix *XMatrix);
   GBarAccelerator *CreateRegionGBA(int nr, cdouble Omega, double *kBloch,
                                    double RMin[3], double RMax[3], bool ExcludeInnerCells);

   void GetUnitCellRepresentative(const double X[3], double XBar[3],
                                  bool WignerSeitz=false);
   void GetUnitCellRepresentative(const double X[3], double XBar[3],
                                  double LVector[3],
                                  bool WignerSeitz=false);
   void GetUnitCellRepresentative(const double X[3], double XBar[3],
                                  double LVector[3], int NVector[3],
                                  bool WignerSeitz=false);

   void GetKNCoefficients(HVector *KN, int ns, int ne,
                          cdouble *KAlpha, cdouble *NAlpha=0);
   RWGSurface *ResolveEdge(int neFull, int *pns=0, int *pne=0, int *pKNIndex=0);
   int UnResolveEdge(int ns, int ne);
   RWGSurface *ResolveBF(int nbfFull, int *pns=0, int *pne=0, bool *pIsMagnetic=0);

   /*--------------------------------------------------------------*/
   /*- private data fields  ---------------------------------------*/ 
   /*--------------------------------------------------------------*/
//private:

   int NumRegions;
   char **RegionLabels;
   MatProp **RegionMPs;

   // cached values of epsilon and mu for each region
   // 'EpsTF' = 'epsilon, this frequency'
   cdouble *EpsTF, *MuTF;
   cdouble StoredOmega;

   int NumSurfaces;
   RWGSurface **Surfaces;
   int AllSurfacesClosed;

   int TotalBFs;
   int TotalEdges;
   int TotalPanels;
   double AveragePanelArea;
   double tolVecClose; // absolute tolerance for VecClose

   char *GeoFileName;

   void **FIBBICaches;

   /**************************************************************/
   /* LDim=0 for compact geometries.                             */
   /* For geometries with D-dimensional Bloch-periodicity,       */
   /* LBasis is a 3 x D matrix whose columns are the lattice     */
   /* basis vectors.                                             */
   /* RLBasis is a similar matrix for the reciprocal lattice.    */
   /* LVolume, RLVolume are the unit-cell volumes of the direct  */
   /* and reciprocal lattices.                                   */
   /*                                                            */
   /* All other fields in this section are only used for PBCs:   */
   /*                                                            */
   /* NumStraddlers[nd][ns] = number of straddlers on unit-cell  */
   /*                         face normal to lattice vector #nd  */
   /*                         for surface #ns                    */
   /* RegionIsExtended[nd][ns] = true if the periodicity lattice */
   /*                            of region #nr includes the #ndth*/
   /*                            lattice vector                  */
   /**************************************************************/
   int LDim;
   HMatrix *LBasis, *RLBasis;
   double LVolume, RLVolume;
   int *NumStraddlers[MAXLDIM];
   bool *RegionIsExtended[MAXLDIM];

   /* BFIndexOffset[n] is the index within the overall BEM          */
   /* system vector of the first basis function on surface #n. thus */
   /*  BFIndexOffset[0]=0                                           */
   /*  BFIndexOffset[1]=# BFs for surface 0                         */
   /*  BFIndexOffset[2]=# BFs for surface 0 + # BFs for surface     */
   /* etc.                                                          */
   int *BFIndexOffset;
   int *EdgeIndexOffset;
   int *PanelIndexOffset;

   /* Mate[i]=j if j<i and surface j is identical to surface i. */
   /* Mate[i]=-1 if there is no surface identical to surface i, */
   /* OR if there is an surface identical to surface i but its  */
   /* index is greater than i.                                  */
   /* for example, if surfaces 2 and 3 are identical then       */
   /*  Mate[2]=-1, Mate[3]=2.                                   */
   /* note: two surfaces are identical if  	                */
   /*  (1) they have the same mesh file (and the same physical  */
   /*      region in that mesh file)                            */
   /*  (1) the regions on either side have the same material    */
   /*      properties                                           */
   int *Mate;

   // EEPTables[nsa][nsb] = equivalent edge-pair table for surfaces (nsa,nsb) 
   std::vector < std::vector< EquivalentEdgePairTable *> > EEPTables;

   /* SurfaceMoved[i] = 1 if surface #i was moved on the most   */
   /* recent call to Transform(). Otherwise SurfaceMoved[i]=0.  */
   int *SurfaceMoved;

   /* Each MMJData structure describes a single triangle edge */
   /* at which two or more distinct surfaces meet.            */
   MMJData **MultiMaterialJunctions;
   int NumMMJs;

   /* Substrate is non-NULL iff a multilayered substrate is present.*/
   LayeredSubstrate *Substrate;
  
   int LogLevel; 
   const char *TBlockCacheNameAddendum;
   
   static bool UseHRWGFunctions;
   static bool UseHighKTaylorDuffy;
   static bool UseTaylorDuffyV2P0;
   static bool DisableCache;
 };

/***************************************************************/
/* non-class methods that operate on RWGPanels and RWGSurfaces */
/***************************************************************/
int CountCommonRegions(RWGSurface *Sa, RWGSurface *Sb, 
                       int CommonRegionIndices[2], double Signs[2]);

/* routines for creating the 'Gamma Matrix' used for torque calculations */
void CreateGammaMatrix(double *TorqueAxis, double *GammaMatrix);
void CreateGammaMatrix(double TorqueAxisX, double TorqueAxisY, 
                       double TorqueAxisZ, double *GammaMatrix);
void CreateGammaMatrix(double Theta, double Phi, double *GammaMatrix);

/***************************************************************/
/***************************************************************/
/***************************************************************/
void *CreateFIBBICache(char *MeshFileName);
void DestroyFIBBICache(void *pCache);
int GetFIBBICacheSize(void *pCache, int *pHits, int *pMisses);
void StoreFIBBICache(void *pCache, const char *MeshFileName);
void GetFIBBIData(void *pCache,
                  RWGSurface *SA, int neA, RWGSurface *SB, int neB,
                  double *FIBBIs);

/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/
void PreloadCache(const char *FileName);
void StoreCache(const char *FileName);
void CheckLattice(HMatrix *LBasis);

HMatrix *MakeMeshPlot(MeshDataFunc MDFunc, void *UserData,
                      const char *MeshFile, const char *TransFile=0,
                      const char *OutFileBase=0, const char *PPOptions=0,
                      bool UseCentroids=true);

HVector *MakeMeshPlot(MeshDataFunc MDFunc, void *UserData,
                      dVec X0, dVec L1, dVec L2, iVec NVec,
                      const char *FileBase, const char *PPOptions=0, bool UseCentroids=false);
                  
} // namespace scuff

#endif // #ifndef LIBSCUFF_H
